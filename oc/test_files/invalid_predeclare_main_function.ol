/**
 * Author: Jack Robbins
 * Test an invalid attempt to predeclare the main function. This is really just a sanity check
 * to make sure that we block this
 */


//BAD! - we cannot ever predeclare the main function
declare pub fn main(argc:i32, argv:char**) -> i32;



pub fn main(argc:i32, argv:char**) -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
