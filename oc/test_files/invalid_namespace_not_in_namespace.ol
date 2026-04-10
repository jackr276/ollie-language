/**
* Author: Jack Robbins
* Test the case where we have an invalid fully qualified name that the user is attempting to use directly
*/


namespace my_namespace 
{
	namespace duplicate 
	{
		pub fn tester() -> i32 {
			ret 5;
		}
	}
}


pub fn main() -> i32 {
	//"not_there" isn't there so we fail
	ret @my_namespace::not_there::tester();
}
