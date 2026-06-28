/**
* Author: Jack Robbins
* Test a case where we have an if assignment with a byte-wide destination. Because of the way that
* x86 conditional moves function, we are not able to use byte-wide destinations and we'll have to convert it to a word
* long destination instead
*/

pub fn byte_sized(x:i8, y:i8) -> i8 {
	ret x > y ? x else y;
}


pub fn main() -> i32 {
	OUNIT: [console = 5]
	ret @byte_sized(5, 4);
}
