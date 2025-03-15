#file TEST_PROG_12;

/**
 * For multi-layer array access
 */
fn main() -> i32{
	let mut i:u32 := 333;

	for(let mut i:u32 := 0; i < 32332; i++) do {
		i := 33;
	}


	ret 0;
}
