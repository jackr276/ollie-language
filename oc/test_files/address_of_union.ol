/**
* Author: Jack Robbins
* Simple testing for the address of a union
*/

define union my_union {
	x:i32;
	y:i64;
}; 

pub fn mut_union(u:mut union my_union*) -> void {
	u->x = 33;
}

pub fn union_mutatation() -> i32 {
	declare x:mut union my_union;

	let y:mut union my_union* = &x;
	
	@mut_union(y);

	ret y->x;
}

pub fn main() -> i32 {
	ret 0;
}
