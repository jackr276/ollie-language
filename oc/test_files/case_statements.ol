/**
* This program is made for the purposes of testing case statements
*/

fn main(arg:i32, argv:char**) -> i32{
	let mut x:i32 := 32;

	switch on(x){
		case 2:
			if(x == 333) then {
				x := 32;
			} else {
				x := 18;
			}
		case 1:
			x := -3;
		case 3:
			x := 211;
		case 5:
			x := 22;
		default:
		//	x := x - 22;
	}

	idle;
	
	
	switch on(x){
		default:
			let i:i32 := 2;
		case 2:
			let i:i32 := 3;
	}
	
	

	//So it isn't optimized away
	ret x;
}
