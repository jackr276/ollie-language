/**
* Author: Jack Robbins
* Test the case where we have an anonymous union defined inside of a struct
*/


define struct my_struct {
	a:mut i32;
	b:mut i64;
	c:define union {
		a:mut i32;
		b:mut i64;
	};
};


pub fn main() -> i32 {
	declare tester:struct my_struct;

	tester:a = 5;
	tester:b = 7;
	tester:c.a = 11;

	OUNIT: [exit_status = 16]
	ret tester:a + tester:c.a;
}
