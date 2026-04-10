/**
* Author: Jack Robbins
* Use the namespace feature to validly call two functions with the same name - "tester"
*/

namespace test_env 
{
	namespace namespace1
	{
		pub fn tester() -> i32 {
			ret 5;
		}
	}
	

	namespace namespace2
	{
		
		pub fn tester() -> i32 {
			ret 3;
		}
	}
}


//Should return 8 in the end
pub fn main() -> i32 {
	ret @test_env::namespace1::tester() + @test_env::namespace2::tester();
}
