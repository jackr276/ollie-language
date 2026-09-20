/**
 * Author: Jack Robbins
 * Test the use of a static function pointer
 */


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn reentrant_helper(x:i32, y:i32) -> i32 {
	let static call_counter:mut i32 = 0;
	declare static fn_ptr:mut fn(i32, i32) -> i32;
	
	//Result changes every time
	declare result:i32;

	//First time we call set the ptr
	if(call_counter == 0){
		fn_ptr = add;
		result = 0;

	//Any other time call it
	} else {
		result = @fn_ptr(x, y);
	}

	//Increase the call counter
	call_counter++;

	ret result;
}


pub fn main() -> i32 {
	//First call means nothing
	@reentrant_helper(0, 0);

	OUNIT: [exit_status = 19]
	ret @reentrant_helper(-8, 27);
}

