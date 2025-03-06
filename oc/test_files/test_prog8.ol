#file test_prog8.ol;

/**
 * For switch statement testing
 */
fn main() -> i32{

	let mut x:i32 := -2U;

	switch on(x){
		case 1:
		case 2:
		case 3:
			{
			x := x + 3;
			break;
			}
		default:
			x := x + 3;
	}

	ret 0;
}
