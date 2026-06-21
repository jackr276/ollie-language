/**
* Author: Jack Robbins
* Test a basic ollie in statement where we are using it inside of a conditional
*/


pub fn in_in_use(x:i32) -> i32 {
	if(x in (1, 3, 4, 7, 11)) {
		ret ++x;
	} else {
		ret --x;
	}
}


pub fn main() -> i32 {
	//Should return 8 + 4 = 12
	OUNIT: [console = 12]
	ret @in_in_use(7) + @in_in_use(5);
}
