/**
 * Author: Jack Robbins
 * Test our ability to initialize a portion of a struct using an initailizer
 */

define struct my_struct {
	x:i32;
	y:i32;
	arr:mut i32[5];
};

pub fn main() -> i32 {
	declare x:struct my_struct;

	//Initialize partially with an initializer
	x:arr = [1, 2, 3, 4, 5];

	OUNIT: [exit_status = 4]
	ret x:arr[3];
}
