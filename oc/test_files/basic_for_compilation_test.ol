/**
* Author: Jack Robbins
* This is meant to be the most basic proof of concept compilation test. We will
* have a main method that calls a looper method with argc as a parameter. We will
* then count to argc + 1 and return
*/

pub fn argc_plus_one(input:i32) -> i32 {
	let result:mut i32 = 1;

	for(let i:mut i32 = 0; i < input; i++){
		result++;
	}

	ret result;
}



pub fn main(argc:i32, argv:char**) -> i32 {
	if(argc < 2) {
		ret 0;
	} else {
		ret @argc_plus_one(argv[1][0]);
	}
}
