#include <iostream>
#include <filesystem>
#include <dpp/dpp.h>
#include <atomic>

#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"
#include "MainLayer.h"

#pragma execution_character_set("utf-8")

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Walnut Example";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<MainLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	
	return app;
}
