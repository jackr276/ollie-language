/**
* Here we will test bad chars in idents
*/

fn:static main() -> i32{
	declare a:u32;
	a := 23;
	let b:u32 := 32;

	ret a + b;
}
