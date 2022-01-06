#![allow(dead_code)]

use std::convert::TryInto;
use taws_primitives::Position;
use uom::si::angle::degree;
use uom::si::f64::{Angle, Length, Velocity, Time};
use uom::si::length::foot;
use uom::si::time::second;
use uom::si::velocity::foot_per_second;

#[derive(Clone)]
pub(crate) struct ClearanceQuery {
    pub start: Position,
    pub pitch: Angle,
    pub bearing: Angle,
    pub speed: Velocity,
    pub dur: Time,
    pub res: Length,
}

impl ClearanceQuery {
    pub(crate) fn to_u32(&self) -> [u32; 16] {
        let mut bytes = vec![];
        bytes.append(&mut self.start.lat.get::<degree>().to_be_bytes().to_vec());
        bytes.append(&mut self.start.lon.get::<degree>().to_be_bytes().to_vec());
        bytes.append(&mut self.start.alt.get::<foot>().to_be_bytes().to_vec());

        bytes.append(&mut self.pitch.get::<degree>().to_be_bytes().to_vec());
        bytes.append(&mut self.bearing.get::<degree>().to_be_bytes().to_vec());

        bytes.append(&mut self.speed.get::<foot_per_second>().to_be_bytes().to_vec());

        bytes.append(&mut self.dur.get::<second>().to_be_bytes().to_vec());

        bytes.append(&mut self.res.get::<foot>().to_be_bytes().to_vec());

        let mut out = [0; 16];
        for (i, u) in bytes.chunks(4).enumerate() {
            out[i] = u32::from_be_bytes(u.try_into().unwrap());
        }
        out
    }

    pub(crate) fn from_u32(data: [u32; 16]) -> ClearanceQuery {
        let mut bytes = vec![];
        for u in data {
            bytes.append(&mut u.to_be_bytes().to_vec());
        }

        let mut data = [0_f64; 8];
        for (i, f) in bytes.chunks(8).enumerate() {
            data[i] = f64::from_be_bytes(f.try_into().unwrap());
        }

        ClearanceQuery {
            start: Position {
                lat: Angle::new::<degree>(data[0]),
                lon: Angle::new::<degree>(data[1]),
                alt: Length::new::<foot>(data[2]),
            },
            pitch: Angle::new::<degree>(data[3]),
            bearing: Angle::new::<degree>(data[4]),
            speed: Velocity::new::<foot_per_second>(data[5]),
            dur: Time::new::<second>(data[6]),
            res: Length::new::<foot>(data[7]),
        }
    }

    pub(crate) fn length_to_u32(len: Length) -> [u32; 2] {
        len.get::<foot>().to_be_bytes().to_vec()
            .chunks(4).map(|i| u32::from_be_bytes(i.try_into().unwrap())).collect::<Vec<_>>()
            .try_into().unwrap()
    }

    pub(crate) fn u32_to_length(u: [u32; 2]) -> Length{
        Length::new::<foot>(f64::from_be_bytes(u.iter().flat_map(|i| i.to_be_bytes()).collect::<Vec<_>>().try_into().unwrap()))
    }
}
