/**
 * Author: Jack Robbins
 * Testing for a relatively complex 3d array intializer
 */


pub fn main() -> i32 {
	let x:i32[][][] = [[[1, 2], [3, 4]],[[5, 6], [7, 8]],[[9, 10], [11, 12]]];

	OUNIT: [exit_status = 12]
	ret x[2][1][1];
}
