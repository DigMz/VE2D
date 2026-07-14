#include "app/app.hpp"

int main()
{
  std::cout << "HelloTriangleApplication" << std::endl;

	try
	{
		Application app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
