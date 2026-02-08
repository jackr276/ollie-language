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
	jump label1;

	//Example asm inline statement
	defer {
		asm{"
			push %rax
			push %rbx
			mov $2, %rax
			addl $3, %rax
			pop %rbx
			pop %rax
			"
		};
	}

	defer {
		x + 3;	
	}
	
	if(!x)  {

	#label1:
		++x;
	} else {
		--x;
		jump label1;
		ret x;
	}
	
	let y:i32 = 32;

	ret 0;
}
