//! How the SDK reaches a master: hidapi in use, a fake master in tests.

use std::ffi::CString;

use crate::error::{Error, Result};
use crate::protocol::{PACKET_SIZE, PID, PRODUCT, USAGE, USAGE_PAGE, VID};

/// One open handle on a pad's vendor interface. Each pad has two: the reader
/// thread owns one, callers write through the other.
pub(crate) trait Link: Send {
    /// One packet into `buf`; Ok(0) if none arrived within `timeout_ms`.
    fn read(&mut self, buf: &mut [u8], timeout_ms: i32) -> Result<usize>;
    /// One 64-byte packet, without the report ID.
    fn write(&mut self, packet: &[u8; PACKET_SIZE]) -> Result<()>;
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
        let reader = self.api.open_path(&c)?;
        let writer = self.api.open_path(&c)?;
        Ok((Box::new(HidLink(reader)), Box::new(HidLink(writer))))
    }
}

struct HidLink(hidapi::HidDevice);

impl Link for HidLink {
    fn read(&mut self, buf: &mut [u8], timeout_ms: i32) -> Result<usize> {
        Ok(self.0.read_timeout(buf, timeout_ms)?)
    }

    fn write(&mut self, packet: &[u8; PACKET_SIZE]) -> Result<()> {
        // The interface has no report IDs, so hidapi wants a 0 in front.
        let mut out = [0u8; PACKET_SIZE + 1];
        out[1..].copy_from_slice(packet);
        self.0.write(&out)?;
        Ok(())
    }
}
