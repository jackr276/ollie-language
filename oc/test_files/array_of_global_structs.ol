/**
* Author: Jack Robbins
* Test an array of global structs and how we handle it. The structs are designed to need
* a lot of padding for alignment
*/


/**
* Struct should be: 1 + 7 pad + 8 + 1 + 7 pad + 8 = 32 bytes large
*/
define struct my_struct {
	a:mut char;
	b:mut i64;
	c:mut i8;
	d:mut f64;
};


let array_of_global_structs:struct my_struct[] = [{'a', 12, 9, 4.44d}, {'b', 13, 12, 5.55d}, {'c', 14, 20, 7.77d}];


pub fn main() -> i32 {
	//Should return 9 + 98('b') + 20 = 127 
	OUNIT: [exit_status = 127]
	ret array_of_global_structs[0]:c + array_of_global_structs[1]:a + array_of_global_structs[2]:c;
	
}
