/**
* Author: Jack Robbins
* Test an in statement that can be live evaluated by the parser
*/


pub fn main() -> i32 {
	OUNIT: [exit_status = 1]
	ret 5 in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}
