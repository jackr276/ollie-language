/**
* Author: Jack Robbins
* Test the case where we have general purpose operations going to floats.
* This is what the converting move emitter was designed to handle
*/

pub fn logical_or_to_float(x:i32, y:i32) -> f32 {
	ret x || y;
}


pub fn logical_and_to_float(x:i32, y:i32) -> f32 {
	ret x && y;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
