//! Events that must not be missed, in one queue for every pad
//! (`docs/HOST_SOFTWARE.md` → "how the SDK delivers data").

use std::collections::VecDeque;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::time::{Duration, Instant};

use crate::messages::ConfigState;

/// Every event names its pad by player ID (the master's DIP, 0–3 = P1–P4).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Event {
    /// A pad was found and answered; its [`Pad`](crate::Pad) is ready.
    PadConnected {
        player: u8,
    },
    PadDisconnected {
        player: u8,
    },
    /// A master was found but not taken on. `player` is `None` for a master
    /// on another protocol version, whose device info isn't read past the
    /// version.
    PadRejected {
        player: Option<u8>,
        reason: RejectReason,
    },
    /// The master found no valid saved config at boot and runs on defaults
    /// until a config is saved: worth suggesting a recalibration. Sent after
    /// [`Event::PadConnected`] while the master's flag is set.
    ConfigWasReset {
        player: u8,
    },
    /// A panel started or stopped answering the master's polls.
    PanelPresence {
        player: u8,
        panel: u8,
        present: bool,
    },
    /// Where a panel's config now stands, with the CRC of the config the panel
    /// reported running (compare [`PanelConfig::crc`](crate::PanelConfig::crc)).
    ConfigResult {
        player: u8,
        panel: u8,
        state: ConfigState,
        crc: u32,
    },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RejectReason {
    /// Another connected pad already has this player ID: change one DIP.
    DuplicatePlayer,
    /// Player IDs 4–7 are reserved. A master straight from assembly reads 7
    /// (all switches off); P1 is all three on.
    ReservedPlayer,
    /// The master speaks another protocol version than this SDK: update one
    /// of them.
    Protocol { version: u8 },
}

pub(crate) type WakeFn = dyn Fn() + Send + Sync;

/// Bounded, so a program that never reads it can't grow it forever; past the
/// bound the oldest event is dropped and counted.
pub(crate) struct EventQueue {
    q: Mutex<VecDeque<Event>>,
    cv: Condvar,
    wake: Mutex<Option<Arc<WakeFn>>>,
    pub dropped: AtomicU64,
}

const MAX_EVENTS: usize = 4096;

impl EventQueue {
    pub fn new() -> Self {
        EventQueue {
            q: Mutex::new(VecDeque::new()),
            cv: Condvar::new(),
            wake: Mutex::new(None),
            dropped: AtomicU64::new(0),
        }
    }

    /// Queues without calling the wake function, so a caller may still hold
    /// its own locks; call [`EventQueue::wake`] once they are released.
    pub fn push(&self, e: Event) {
        let mut q = self.q.lock().unwrap();
        if q.len() == MAX_EVENTS {
            q.pop_front();
            self.dropped.fetch_add(1, Ordering::Relaxed);
        }
        q.push_back(e);
        self.cv.notify_all();
    }

    pub fn wake(&self) {
        let f = self.wake.lock().unwrap().clone();
        if let Some(f) = f {
            f();
        }
    }

    pub fn set_wake(&self, f: Option<Arc<WakeFn>>) {
        *self.wake.lock().unwrap() = f;
    }

    pub fn poll(&self) -> Option<Event> {
        self.q.lock().unwrap().pop_front()
    }

    pub fn wait(&self, timeout: Duration) -> Option<Event> {
        let deadline = Instant::now() + timeout;
        let mut q = self.q.lock().unwrap();
        loop {
            if let Some(e) = q.pop_front() {
                return Some(e);
            }
            let left = deadline.checked_duration_since(Instant::now())?;
            q = self.cv.wait_timeout(q, left).unwrap().0;
        }
    }
}
