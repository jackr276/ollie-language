/**
* Author: Jack Robbins
* This program is a demonstration of a complex memory copy using a 
* more advanced struct than usual. This struct should be 88 bytes in size
*
* We are going to test how well our tracking works. We should be aligning 
* every function that is potentially on the path to memory copying a struct
*/

define struct my_struct {
	x:i32;
	y:i32[20];
	z:f32;
} as complex_struct;


pub fn copy_struct() -> i32 {
	//Declare the original/copy structs
	declare copy:mut complex_struct;
	let original:complex_struct = {5, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20], 2.22};

	//Copy operation
	copy = original;

	//This should return 6 from the array
	ret copy:y[5];
}

pub fn hop3() -> i32 {
	ret @copy_struct();
}

pub fn hop2() -> i32 {
	ret @hop3();
}

pub fn hop1() ->i32 {
	ret @hop2();
}


//Once we're done echo $? should be 6 if all is correct
pub fn main() -> i32 {
	ret @hop1();
}
