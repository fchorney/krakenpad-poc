//! How the SDK reaches a master: hidapi in use, a fake master in tests.

use std::ffi::CString;

use crate::error::{Error, Result};
use crate::protocol::{
    OUT_PACKET_MAX, OUT_PACKET_MIN, PID, PRODUCT, USAGE, USAGE_PAGE, VID, out_report_size,
};

/// One open handle on a pad's vendor interface. Each pad has two: the reader
/// thread owns one, callers write through the other.
pub(crate) trait Link: Send {
    /// One packet into `buf`; Ok(0) if none arrived within `timeout_ms`.
    fn read(&mut self, buf: &mut [u8], timeout_ms: i32) -> Result<usize>;
    /// One packet of [`Link::out_packet`] bytes, without the report ID.
    fn write(&mut self, packet: &[u8]) -> Result<()>;
    /// The size of a packet to the master: 1024 at High Speed, 64 at Full
    /// Speed, from the interface's report descriptor.
    fn out_packet(&self) -> usize;
}

pub(crate) trait Opener: Send {
    /// Paths of every KrakenPad vendor interface plugged in now.
    fn scan(&mut self) -> Vec<String>;
    /// The (reader, writer) pair for one path.
    fn open(&mut self, path: &str) -> Result<(Box<dyn Link>, Box<dyn Link>)>;
}

/// The SDK's own hidapi context. hidapi 2.6 allows several in one process, so
/// this doesn't clash with a game's other HID code.
pub(crate) struct HidOpener {
    api: hidapi::HidApi,
}

impl HidOpener {
    pub fn new() -> Result<Self> {
        Ok(HidOpener {
            api: hidapi::HidApi::new()?,
        })
    }
}

impl Opener for HidOpener {
    fn scan(&mut self) -> Vec<String> {
        if self.api.refresh_devices().is_err() {
            return Vec::new();
        }
        self.api
            .device_list()
            .filter(|d| {
                d.vendor_id() == VID
                    && d.product_id() == PID
                    && d.usage_page() == USAGE_PAGE
                    && d.usage() == USAGE
                    && d.product_string() == Some(PRODUCT)
            })
            .map(|d| d.path().to_string_lossy().into_owned())
            .collect()
    }

    fn open(&mut self, path: &str) -> Result<(Box<dyn Link>, Box<dyn Link>)> {
        let c = CString::new(path).map_err(|_| Error::Hid("path contains NUL".into()))?;
        let reader = HidLink::open(&self.api, &c)?;
        let writer = HidLink::open(&self.api, &c)?;
        Ok((Box::new(reader), Box::new(writer)))
    }
}

struct HidLink {
    dev: hidapi::HidDevice,
    out_packet: usize,
    /// The report ID byte (0: the interface has none) and one packet.
    out: Vec<u8>,
}

impl HidLink {
    fn open(api: &hidapi::HidApi, path: &std::ffi::CStr) -> Result<Self> {
        let dev = api.open_path(path)?;
        // HID allows report descriptors up to 4096 bytes; the master's is 29.
        let mut desc = vec![0u8; 4096];
        let n = dev.get_report_descriptor(&mut desc)?;
        let out_packet = out_report_size(&desc[..n])
            .filter(|s| (OUT_PACKET_MIN..=OUT_PACKET_MAX).contains(s))
            .ok_or_else(|| {
                Error::Hid("unexpected output report in the report descriptor".into())
            })?;
        Ok(HidLink {
            dev,
            out_packet,
            out: vec![0; 1 + out_packet],
        })
    }
}

impl Link for HidLink {
    fn read(&mut self, buf: &mut [u8], timeout_ms: i32) -> Result<usize> {
        Ok(self.dev.read_timeout(buf, timeout_ms)?)
    }

    fn write(&mut self, packet: &[u8]) -> Result<()> {
        // The interface has no report IDs, so hidapi wants a 0 in front.
        self.out[1..].copy_from_slice(packet);
        self.dev.write(&self.out)?;
        Ok(())
    }

    fn out_packet(&self) -> usize {
        self.out_packet
    }
}
