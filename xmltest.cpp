#include <iostream>
#include <string>
#include "tinyxml.h"
#include "tinystr.h"

using std::string;

int main_xml()
{
	TiXmlDocument* myDocument = new TiXmlDocument();
	myDocument->LoadFile("E://FashionRoom//test//3 - points//pic - 3points - new//annotation.xml");
	TiXmlElement* rootElement = myDocument->RootElement();  //Class
	TiXmlElement* studentsElement = rootElement->FirstChildElement();  //Students
//	TiXmlElement* studentElement = studentsElement->FirstChildElement();  //Students
	while (studentsElement) {
		TiXmlAttribute* attributeOfStudent = studentsElement->FirstAttribute();  //获得student的name属性
		while (attributeOfStudent) {
			std::cout << attributeOfStudent->Name() << " : " << attributeOfStudent->Value() << std::endl;
			attributeOfStudent = attributeOfStudent->Next();
		}
		TiXmlElement* phoneElement = studentsElement->FirstChildElement();//获得student的phone元素
		while (phoneElement != NULL){
			std::cout << "X" << " : " << phoneElement->FirstAttribute()->Value() << std::endl;
			TiXmlAttribute* addressElement = phoneElement->FirstAttribute()->Next();
			std::cout << "Y" << " : " << addressElement->Value() << std::endl;
			phoneElement = phoneElement->NextSiblingElement();
		}

		studentsElement = studentsElement->NextSiblingElement();
	}
	return 0;
}