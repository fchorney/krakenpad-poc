use std::fmt;

pub type Result<T> = std::result::Result<T, Error>;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    /// hidapi failed (open, read or write); the text is hidapi's.
    Hid(String),
    /// The pad is gone: unplugged, or closed while the call waited.
    Disconnected,
    /// No response in time.
    Timeout,
    /// The master sent something this SDK can't read.
    Protocol(&'static str),
    /// The master refused the command.
    Device { command: u8, reason: DeviceError },
    /// A channel breaks `release < press <= 4095`. Checked before sending.
    InvalidConfig { panel: u8, channel: u8 },
    /// The master speaks another config format version.
    ConfigVersion { master: u8, sdk: u8 },
    /// The master pushed the config, but its EEPROM read-back didn't match,
    /// so it won't survive a master reboot.
    SaveFailed,
}

/// Why the master refused a command (`docs/USB_PROTOCOL.md` → "As built").
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DeviceError {
    UnknownCommand,
    BadLength,
    /// Not now: a bus command during passthrough, for example.
    Busy,
    /// Not in this master firmware yet.
    Unsupported,
    /// A status this SDK doesn't know.
    Other(u8),
}

impl DeviceError {
    pub(crate) fn from_code(c: u8) -> Self {
        match c {
            1 => DeviceError::UnknownCommand,
            2 => DeviceError::BadLength,
            3 => DeviceError::Busy,
            4 => DeviceError::Unsupported,
            other => DeviceError::Other(other),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Hid(e) => write!(f, "HID error: {e}"),
            Error::Disconnected => f.write_str("pad disconnected"),
            Error::Timeout => f.write_str("no response from the pad in time"),
            Error::Protocol(what) => write!(f, "unexpected data from the pad: {what}"),
            Error::Device { command, reason } => {
                write!(f, "pad refused command {command:#04x}: {reason:?}")
            }
            Error::InvalidConfig { panel, channel } => {
                write!(
                    f,
                    "panel {panel} channel {channel}: release must be below press, press at most 4095"
                )
            }
            Error::ConfigVersion { master, sdk } => {
                write!(
                    f,
                    "the master speaks config version {master}, this SDK {sdk}"
                )
            }
            Error::SaveFailed => f.write_str("config sent, but the master could not save it"),
        }
    }
}

impl std::error::Error for Error {}

impl From<hidapi::HidError> for Error {
    fn from(e: hidapi::HidError) -> Self {
        Error::Hid(e.to_string())
    }
}
