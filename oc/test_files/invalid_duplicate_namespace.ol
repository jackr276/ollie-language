/**
* Author: Jack Robbins
* This test file will validate that we can detect and reject duplicate namespaces under the
* same parent
*/


namespace my_namespace 
{
	namespace duplicate 
	{
		pub fn tester() -> i32 {
			ret 5;
		}
	}

	//Should fail
	namespace duplicate 
	{
		pub fn tester() -> i32 {
			ret 2;
		}
	}
}


pub fn main() -> i32 {
	ret 0;
}
