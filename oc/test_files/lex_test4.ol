/**
* Here we will test bad chars in idents
*/

fn:static main() -> i32{
	declare u32 a;
	let u32 a := 23;
	let u32 b := 32;

	ret a + b;
}
