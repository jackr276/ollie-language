
/**
 * Testing do-while
 */
fn main() -> i32{
	let mut i:u32 := 333;

	for(let mut i:u32 := 333; i < 324252; i++) do{
		declare u:i32;
		i := i - 3;
		continue when(i == 33);

	}

	ret 0;
}
