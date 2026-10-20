/**
 * Jack Robbins
 * Contrived example to verify that we can't assign to an initializer
 */

pub fn main() -> i32 {
	let x:i32 = 5;

	
	//BAD(obviously)
	(x in (1, 2, 3, 4)) = 5;
	

	OUNIT: [fail_to_compile]
	ret 0;
}
