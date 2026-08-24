/**
 * Author: Jack Robbins
 * Test our ability to inline functions that contain assembly statements
 */

inline fn inline_asm() -> void {
	//This really has no effect it reverses itself
	asm {
		"
		pushq %rax
		movq %rcx, %rax
		popq %rax
		"
	};
}



pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;

	@inline_asm();
	@inline_asm();

	OUNIT: [exit_status = 11]
	ret x + y;
}
