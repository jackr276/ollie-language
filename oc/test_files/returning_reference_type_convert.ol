//This should return a reference - there should be no auto-deref
//happening here
fn get_max(a:i32&, b:i32&) -> i32& {
	ret (a > b) ? a else b;
}

fn return_reference_type_widen(a:i32&) -> i64 {
	ret a;
}

fn return_reference_type_widen_arith(a:i32&) -> i64 {
	ret a + 1;
}

pub fn main() -> i32 {
	let x:mut i32 = 10;
	let y:mut i32 = 15;

	//The compiler should implicitly get the addresses of x and y here
	ret @get_max(x, y);
}
