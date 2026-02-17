/**
* Author: Jack Robbins
* Test the most basic case of a macro parameter
*/

//Basic one with a param
$macro INC_BY_5(x)
x += 5
$endmacro


pub fn main() -> i32 {
	let abc:mut i32 = 2;

	//Invoke the paramaterized macro
	INC_BY_5(abc);

	ret abc;
}

