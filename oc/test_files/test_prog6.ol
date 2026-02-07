//Ollie macros
$macro TEST_INT -1 $endmacro
$macro my_char 'c' $endmacro

fn tester() -> void{
	let x:mut i32 = !3;
	++x;
	ret;
}

/**
 * Demonstrate the functionality of saturating add for positive and negative overflows
 */
pub fn main() -> i32{
	let x:mut i32 = -2U;
	let aa:mut i32 = --3;
	let my_val:mut i32 = x + -32;
	let teste:mut i32 = TEST_INT;
	let test_char:char = my_char;

	defer {
		x++;	
	}
	
	if(!x) {
		++x;
	} else {
		--x;
		ret x;
	}
	

	ret x;
}
