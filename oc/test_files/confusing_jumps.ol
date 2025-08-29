/**
* Author: Jack Robbins
* Extreme edge case testing for jumping. Should anyone ever do something like this? Absolutely not.
* But it is officially supported so we have to validate
*/


fn confusing_jumps(mut a:i32, mut b:i32) -> i32{
	if(a == 3){
		jump end_label;

#internal_label:
		a--;
	}

#end_label:
	if(b == 2){
		b--;
		jump internal_label;
	}

	//So they're both needed
	ret a + b;
}


pub fn main() -> i32 {
	ret @confusing_jumps(3, 5);
}
