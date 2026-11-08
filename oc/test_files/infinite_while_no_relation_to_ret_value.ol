/**
 * Author: Jack Robbins
 * Test an infinite while loop where all of the loop contents have no relation to the final
 * return value
 */


pub fn main() -> i32 {
	let a:mut i32 = 5;
	let b:mut i32 = 15;

	/**
	 * Literally useless - but this may be infinite, so
	 * we need to keep it inside the final CFG
	 */
	while(a != b){
		a++;
	}

	OUNIT: [exit_status = 51]
	ret 51;
}
