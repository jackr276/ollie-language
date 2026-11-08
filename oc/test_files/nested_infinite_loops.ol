/**
 * Author: Jack Robbins
 * Test our ability to preserve nested infinite loops in that lead to the function exit
 * using SCCs and virtual edges
 */

pub fn nested_potentially_infinite(x:mut i32, y:mut i32) -> i32 {
	/**
	 * Even though these loops do nothing of meaning, we still
	 * need to keep them around because they are potentially
	 * nonterminating(i.e., y may never equal x)
	 */
	while(x < 55) {
		while(y != x){
			y++;
		}
		x++;
	}
	
	ret 0;
}


pub fn main() -> i32 {
	@nested_potentially_infinite(0, 12);

	OUNIT: [exit_status = 5]
	ret 5;
}
