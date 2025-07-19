/**
* This program is made for the purposes of testing short circuiting logic
*/

fn tester(arg:i32) -> void{
	arg++;
}

//Dummy
fn really_long_function(arg:i32) -> i32 {
	arg++;
	ret arg;
}

fn main(arg:i32, argv:char**) -> i32{
	let mut x:u32 := 232;

	defer {
		x := x + 3;
		@tester(x);
	};

	
	if(arg < x && x != 1){
		x := x - 3;
	} else if(x < 2 && (x != 1 || x != 2)) {
		x := x * 2;
	} else {
	 	x := x + 3;
		ret x;
	}
	

	//Assign B a start
	let mut b:i32 := 33;

	do{
		b--;
		x := x - 1;
	} while(x == 32 || b <= 32) ;

	//So it isn't optimized away
	ret x;
}
