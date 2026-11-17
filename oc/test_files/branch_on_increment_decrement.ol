/**
 * Author: Jack Robbins
 * Test our ability to branch on increment/decrement operations. In x86 assembly, INC/DEC do not
 * fully set condition codes(specifically the CF flag is not set by them). As such, we do not
 * rely on them for branching and instead insert test if not zero operations after them
 */


pub fn branch_on_inc(x:mut i32) -> i32 {
	if(++x) {
		ret 1;
	} else {
		ret 8;
	}
}


pub fn branch_on_dec(x:mut i32) -> i32 {
	if(--x) {
		ret 1;
	} else {
		ret 5;
	}
}


pub fn main() -> i32 {
	//Should return 1 + 5 = 6
	OUNIT:[exit_status = 8]
	ret @branch_on_inc(0) + @branch_on_dec(1);
}
