/**
* Author: Jack Robbins 
* This test will verify that the compiler can handle parameter interference with
* references(a more unique case)
*/

fn add_vals(x:i32&, y:i32&) -> i32 {
	ret x + y;
}


/**
* This creates the scenario where we could have issues
* with wiping out parameters where they need to be reused
* afterwards
*/
fn swap_add(x:i32&, y:i32&) -> i32 {
	//Swap them
	let sum1:i32 = @add_vals(y, x);

	//Then unswap them
	let sum2:i32 = @add_vals(x, y);

	ret sum1 | sum2;
}


pub fn main() -> i32 {
	let x:i32 = 3;
	let y:i32 = 3;

	//Automatically generates references
	ret @swap_add(x, y);
}
