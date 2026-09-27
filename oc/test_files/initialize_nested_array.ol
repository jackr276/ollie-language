/**
 * Author: Jack Robbins
 * Test our ability to initialize a nested array in a multidimensional array using an initailizer
 */


pub fn main() -> i32 {
	declare x:mut i32[5][4];

	//nested array initialization
	x[3] = [1, 2, 3, 4];
	x[4] = [1, 2, 3, 4];
	

	//Should return 2 + 3 = 5
	OUNIT: [exit_status = 5]
	ret x[3][1] + x[4][2];
}
