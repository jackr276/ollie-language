/**
* Author: Jack Robbins
* Test the use of "declare" with global vars
*/

//Global var
declare x:mut i32;


pub fn main() -> i32 {	
	//This should *not* fail. It's not possible for
	//the compiler to know if/when a global var has 
	//been initialized, so declared ones are all initialized
	//to 0 to start anyway
	ret x + 11;
}
