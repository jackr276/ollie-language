/**
 * Author: Jack Robbins
 * Test an invalid case where we try to mutate an immutable function parameter
 */

pub fn test(x:i32, y:i32) -> i32 {
	//BAD - can't change this
	if(x == 5){
		y = 2;
	} else {
		y = 3;
	}

	ret y;
}

pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @test(1, 2);
}
