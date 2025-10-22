/**
* This program is made for the purposes of testing short circuiting logic
*/

fn tester(arg:i32) -> void{
	arg++;
}

//Dummy
fn test_function(arg:i32) -> i32 {
	ret arg == 4;
}

fn test_function2(arg:i32) -> i32 {
	let x:i32 = arg == 3;
	ret x;
}


fn test_while_or(arg:i32) -> i32 {
	let mut x:i32 = 55;

	//Will trigger inverse
	while(x && arg) {
		x--;
	}

	ret x;
}


fn test_while_and(arg:i32) -> i32 {
	let mut x:i32 = 55;

	//Will trigger inverse
	while(x && arg) {
		x--;
	}

	ret x;
}

pub fn test_if_and(arg:i32) -> i32 {
	let mut x:u32 = 232;

	if(x || arg){
		x = x - 3;
	} else {
	 	x = x + 3;
		ret x;
	}

	ret x;
}

pub fn main(arg:i32, argv:char**) -> i32{
	let mut x:u32 = 232;
	
	if(x || arg){
		x = x - 3;
	} else {
	 	x = x + 3;
		ret x;
	}

	//So it isn't optimized away
	ret x;
}
