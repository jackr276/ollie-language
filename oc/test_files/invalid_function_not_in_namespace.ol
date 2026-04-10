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
	//Tester1 doesn't exist so we fail out
	ret @my_namespace::duplicate::tester1();
}
