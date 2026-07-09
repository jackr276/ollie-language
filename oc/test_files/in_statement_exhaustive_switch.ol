/**
* Author: Jack Robbins
* Test an instance where we have a so-called "exhaustive switch". A switch with no
* gaps inbetween the values at all. This special type of switch produces no default
* clause at all
*/


pub fn exhaustive_in(x:i32) -> i32 {
	if(x in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10)){
		ret 5;
	} else {
		ret 2;
	}
}


pub fn main() -> i32 {
	//Should return 5 + 2 = 7
	OUNIT: [exit_status = 7]
	ret @exhaustive_in(5) + @exhaustive_in(11);
}
