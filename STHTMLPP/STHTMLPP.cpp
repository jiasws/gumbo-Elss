// STHTMLPP.cpp: 定义控制台应用程序的入口点。
//
#pragma warning (disable : 4996)
#include "stdafx.h"
#include <iostream>

extern "C" {
	#include "sds.h"
	#include<Windows.h>  
	#include<time.h>
}
#include <map>
#include<vector>
#include "gumbo.h"
#include<json/json.h>
#include"Tools.h"
#include"utf8_strings.h"
using namespace Json;

//调试内存泄漏
#include <crtdbg.h>
#define _CRTDBG_MAP_ALLOC

using namespace std;

using namespace litehtml;
#pragma comment(lib,"User32.lib")
#ifdef _DEBUG
//#pragma comment(lib,"gumbo.lib")
#else
#pragma comment(lib,"StaticLib1releasegumbo.lib")
#endif

//--选择器
#define CLASS "class"
#define ID "id"
//--函数
#define Fun_find "find"
#define Fun_fuzzyfind "fuzzyfind"
#define Fun_tojson "tojson"
/*
*
*此宏控制属性字节长度 _tojson函数内
*
*/
#define ATTRLEN 100 
/*
*
*方便导出c接口
*
*/
typedef std::vector<GumboNode*> GumboNodes;

size_t n = 0;
clock_t start, finish;
double totaltime;
/*
*
*通过参数控制递归获取想要的节点
*
*/
void ergodic(vector<GumboNode*> *momo, HTMLengine* engine, GumboNode* Node,const char* path, const char* NodeType, 
	BOOL recursion = true/*申明了递归深度true无限递归反之一层*/, 
	BOOL layer = true,Value *NodeJson = NULL)
{
	if (Node->type != GUMBO_NODE_ELEMENT) {
		return;
	}
	n++;
	if (strlen(path) > 0)//防止里面啥都没有导致解析出错
	{
		if (strcmp(NodeType, Fun_find) == 0)
		{//del_chr
			vector<string> Args = Split((char*)strreplace(path, "'", "").data(), ",");
			if (Args.size() == 2)
			{
				GumboAttribute* name = engine->HTMLNode_GetAttributeByName(Node, Args[0].c_str());
				if (name != NULL)
				{
					if (strcmp(name->value, Args[1].c_str()) == 0)
					{
						momo->push_back(Node);
					}
				}
			}
		}
		if (strcmp(NodeType, Fun_fuzzyfind) == 0)
		{
			vector<string> Args = Split((char*)strreplace(path, "'", "").data(), ",");
			if (Args.size() == 2)
			{
				GumboAttribute* name = engine->HTMLNode_GetAttributeByName(Node, Args[0].c_str());
				if (name != NULL)
				{
					if (strcmp(name->value, Args[1].c_str()) >= 0)
					{
						momo->push_back(Node);
					}
				}
			}
		}
		if (strcmp(NodeType, "class") == 0)
		{
			GumboAttribute* name = engine->HTMLNode_GetAttributeByName(Node, NodeType);
			if (name != NULL)
			{
				if (strcmp(name->value, path) == 0)
				{
					momo->push_back(Node);
				}
			}
		}
		else if (strcmp(NodeType, "id") == 0) {
			GumboAttribute* name = engine->HTMLNode_GetAttributeByName(Node, NodeType);
			if (name != NULL)
			{
				if (strcmp(name->value, path) == 0)
				{
					momo->push_back(Node);
				}
			}
		}
		else {
			if (layer != false)
			{
				if (strcmp(NodeType, gumbo_normalized_tagname(Node->v.element.tag)) == 0)
				{
					momo->push_back(Node);
				}
			}
		}
		if (recursion)//控制递归深度
		{
			size_t n = engine->HTMLNode_GetChildCount(Node);
			GumboVector* children = &Node->v.element.children;
			for (size_t i = 0; i < n; i++)
			{
				ergodic(momo, engine, static_cast<GumboNode*>(children->data[i]), path, NodeType, layer);
			}
		}
	}
}

vector<GumboNode*> CLHQ(HTMLengine* engine, GumboNode* H, char* path, vector<GumboNode*>* list , Json::Value  *NodeJson);
/*
*
*_tojson方法原型，这里处理所有方法，但不修改只提取信息
*
*/
void _tojson(HTMLengine* engine,const char* nam, GumboNode* H, ArrayIndex p,size_t index, Json::Value  *NodeJson = NULL)
{
	char name[ATTRLEN];
	strcpy(name,nam);
	del_chr(name, '\'');
	vector<string> Args = Split(name, ",");
	//Split(nam, ",")
	GumboNode* ThisNode = NULL;
	if (Args.size() == 4)
	{
		if (strcmp(Args[1].c_str(),"this") == 0)
		{
			ThisNode = H;
		}
		else
		{
			/*
			因为语法解析问题嵌套相同语法会解析出问题，所以此部分功能暂时简单实现子元素读取
			下次重构语法解析此功能再加：
			功能记录如下:
			div > a > div:tojson(内容,this,[class+href])
			改为
			div > a > div:tojson(内容,this,[class,href])
			并且实现子元素表达式，例如L：
			div > a > div:tojson(内容,'div>a[0]',[class,href])
			div > a > div:tojson(内容,'div>a[0~5]',[class,href])
			*/
			char* xpath = (char*)Args[1].c_str();
			vector<GumboNode*> HTMLnode = CLHQ(engine, H, xpath,NULL,NULL);
			if (HTMLnode.size()>0)
			{
				ThisNode = HTMLnode[0];
			}
		}
		vector<string> Attrs = Split((char*)strreplace(strreplace(Args[3], "[", ""), "]", "").data(), "+");
		for (auto& n : Attrs)
		{
			if (n.compare("Text") == 0)//如果等于text则输出
			{
				string text = engine->HTMLNode_GetText(ThisNode);
				//插入 {Args[0]：Text:[text]}
				if (p == 0)
				{
					Json::Value person, *person1;
					person[Args[2].c_str()] = Value(text.data());
					NodeJson->resolveReference(G2U(Args[0].c_str())).append(person);
					continue;
				}
				else {
					NodeJson->resolveReference(G2U(Args[0].c_str()))[index][Args[2].c_str()] = Value(text.data());
					continue;
				}
			}
			if (n.compare("Html") == 0)
			{
				string html = engine->HTMLNode_GetHtml(ThisNode);
				//插入 {Args[0]：Html:[html]}
				if (p == 0) {
					Json::Value person, *person1;
					person[Args[2].c_str()] = Value(html.data());
					NodeJson->resolveReference(G2U(Args[0].c_str())).append(person);
				}
				else {
					NodeJson->resolveReference(G2U(Args[0].c_str()))[index][Args[2].c_str()] = Value(html.data());
				}
				continue;
			}
			GumboAttribute* value = engine->HTMLNode_GetAttributeByName(ThisNode, n.c_str());
			if (value != NULL)
			{
				if (p == 0) {
					Json::Value person, *person1;
					person[Args[2].c_str()] = Value(value->value);
					NodeJson->resolveReference(G2U(Args[0].c_str())).append(person);
				}
				else {
					NodeJson->resolveReference(G2U(Args[0].c_str()))[index][Args[2].c_str()] = Value(value->value);
				}
				//value->value
				//插入 {Args[0]：n.c_str():[value]}
			}
		}
	}
}
/*
*
*所有方法这里会处理
*
*/
size_t FunEx(HTMLengine* engine, GumboNode* H, size_t te_1, string NodeName, vector<GumboNode*> &as, BOOL IsIndex,string NodeType, Json::Value  *NodeJson = NULL)
{//解析函数
	string text = NodeName.substr(te_1, NodeName.size() - te_1);
	vector<string> pats = Split((char*)NodeName.data(), ":");
	for (size_t i = 0; i < pats.size(); i++)
	{
		vector<GumboNode*> as_class;
		vector<string> list = Split((char*)pats.at(i).data(), "(");
		string data = list.size() > 0 ? list[0] : "";
		size_t j, x;
		string nam, nam_1;
		size_t te_1 = pats.at(i).find("[");
		if (te_1!=-1)
		{
			x = atoi(data.substr(te_1 + 1, data.find("]") - 1 - te_1).c_str());
			data = data.substr(0, te_1);
		}
		switch (hash_(data.c_str()))
		{
		case "tojson"_hash://提取属性不遍历任何节点
			j = list[1].find(")");
			nam = list[1].replace(j, list[1].size() - j, "");
			if (as.size() > 0)
			{
				string test(nam);
				string test1 = test.substr(0, test.find(","));
				json:ArrayIndex p = NodeJson->resolveReference(G2U(test1.c_str())).size();
				for (size_t i = 0; i < as.size(); i++)
				{
					_tojson(engine, nam.c_str(), as[i], p, i, NodeJson);
				}
			}
			break;
		case "find"_hash://属性筛选
			j = list[1].find(")");
			nam = list[1].replace(j, list[1].size() - j, "");
			if (as.size() > 0)
			{
				for (size_t i = 0; i < as.size(); i++)
				{
					ergodic(&as_class, engine, as.at(i), nam.c_str(), Fun_find, true, i == 0 ? IsIndex : false);
				}
			}
			else {
				ergodic(&as_class, engine, H, nam.c_str(), Fun_find, true, i == 0 ? IsIndex : false);
			}
			if (as_class.size() == 0)
			{
				as.clear();
				return 0;//如果没有命中任何元素,则返回0
			}
			if (te_1 != -1)
			{
				if (x <= (as_class.size() - 1))
				{
					as.clear();
					as.push_back(as_class[x]);
				}
			}
			else {
				as.assign(as_class.begin(), as_class.end());
			}
			break;
		case "fuzzyfind"_hash://模糊筛选
			j = list[1].find(")");
			nam_1 = list[1].replace(j, list[1].size() - j, "");
			if (as.size() > 0)
			{
				for (size_t i = 0; i < as.size(); i++)
				{
					ergodic(&as_class, engine, as.at(i), nam_1.c_str(), Fun_fuzzyfind, true, i == 0 ? IsIndex : false);
				}
			}
			else {
				ergodic(&as_class, engine, H, nam_1.c_str(), Fun_fuzzyfind, true, i == 0 ? IsIndex : false);
			}
			if (as_class.size() == 0)
			{
				as.clear();
				return 0;//如果没有命中任何元素,则返回0
			}
			if (te_1 != -1)
			{
				if (x <= (as_class.size() - 1))
				{
					as.clear();
					as.push_back(as_class[x]);
				}
			}
			else {
				as.assign(as_class.begin(), as_class.end());
			}
			break;
		default://如果没有命中函数
			if (as.size() > 0)
			{
				for (size_t i = 0; i < as.size(); i++)
				{
					ergodic(&as_class, engine, as.at(i), data.c_str(), NodeType.c_str(), true, i == 0 ? IsIndex : false);
				}
			}
			else {
				ergodic(&as_class, engine, H, data.c_str(), NodeType.c_str(), true, i == 0 ? IsIndex : false);
			}
			if (as_class.size() == 0)
			{
				as.clear();
				return 0;
			}
			if (te_1 != -1)
			{
				if (x <= (as_class.size() - 1))
				{
					as.clear();
					as.push_back(as_class[x]);
				}
			}
			else {
				as.assign(as_class.begin(), as_class.end());
			}
			break;
		}
	}
	return 1;
}
/*
*
*处理表达式树
*
*/
int extract_nodes(HTMLengine* engine, GumboNode* H, vector<GumboNode*> &as,string NodeName, string NodeType, Json::Value  *NodeJson = NULL)
{
	vector<GumboNode*> as_class;
	//ergodic(&as_class, engine, H, "a", "a");
	size_t te = NodeName.find(":");
	if (te == -1)
	{
		size_t te_1 = NodeName.find("[");
		if (te_1 != -1)
		{
			size_t x = atoi(NodeName.substr(te_1 + 1, NodeName.find("]") - 1 - te_1).c_str());
			if (as.size() > 0)
			{
				for (size_t ia = 0; ia < as.size(); ia++)
				{
					ergodic(&as_class, engine, as.at(ia), NodeName.substr(0, te_1).c_str(), NodeType.c_str(), true, false);
				}
			}
			else {
				ergodic(&as_class, engine, H, NodeName.substr(0, te_1).c_str(), NodeType.c_str());
			}
			if (as_class.size() == 0)
			{
				as.clear();
				return 0;//如果没有命中任何元素,则返回0
			}
			if (x <= (as_class.size() - 1))
			{
				as.clear();
				as.push_back(as_class[x]);
			}
		}
		else {
			if (as.size() > 0)
			{
				for (size_t i1 = 0; i1 < as.size(); i1++)
				{
					ergodic(&as_class, engine, as.at(i1), NodeName.c_str(), NodeType.c_str(), true, false);
				}
			}
			else {
				ergodic(&as_class, engine, H, NodeName.c_str(), NodeType.c_str());
			}
			if (as_class.size() == 0)
			{
				as.clear();
				return 0;//如果没有命中任何元素,则返回0
			}
			as.assign(as_class.begin(), as_class.end());
		}
	}
	else {
		//--解析表达式
		size_t te_1 = te;
		te_1++;
		FunEx(engine, H, te_1, NodeName, as, true, NodeType, NodeJson);
		if (as.size() == 0)
		{
			return 0;//如果没有命中任何元素,则返回0
		}
		as_class.assign(as.begin(), as.end());
		te_1 = NodeName.find("[");
		if (te_1 != -1) {
			size_t x = atoi(NodeName.substr(te_1 + 1, NodeName.find("]") - 1 - te_1).c_str());
			if (x <= (as_class.size() - 1))
			{
				as.clear();
				as.push_back(as_class[x]);
			}
		}
		else { as.assign(as_class.begin(), as_class.end()); }
		//--
	}
	return 1;
}
/*
*
*表达式方法
*
*/
vector<GumboNode*> CLHQ(HTMLengine* engine, GumboNode* H,char* path, vector<GumboNode*>* list = NULL, Json::Value  *NodeJson = NULL)
{
	//".class > a > div > .s"
	//"#id > a > div > .s"
	//"div > a > div > .s"
	//li>a:tojson(ddd,div>a>b,href)
	//首先解析表达式Document document;
	vector<string> Nodes = GetExpression(path);
	/*char *token;
	const char s[2] = ">";
	token = strtok(path, s);
	while (token != NULL) {
		string token1(token);
		Nodes.push_back(trim(token1));
		token = strtok(NULL, s);
	}*/
	vector<GumboNode*> as;
	if (list != NULL)
		as.assign(list->begin(), list->end());
	for (size_t i = 0; i < Nodes.size(); i++)
	{
		string type = trim(Nodes[i]);
		char a = type.c_str()[0];
		switch (a)
		{
		case '.'://遍历as节点数组寻找满足class节点
			extract_nodes(engine,H, as, type.erase(0, 1), CLASS, NodeJson);
			break;
		case '#'://遍历as节点数组寻找满足id节点
			extract_nodes(engine, H, as, type.erase(0, 1), ID, NodeJson);
			break;
		default://遍历as节点数组寻找满足节点
			if (type.find(":")!=-1)
			{
				type = type.substr(0, type.find(":"));
			}
			extract_nodes(engine, H, as, trim(Nodes[i]), (type.find("[")!=-1)? type.substr(0, type.find("[")): type, NodeJson);
			break;
		}
		if (as.size()<=0)
		{
			return as;
		}
	}
	return as;
}

/*
*
*导出c接口
*
*/
HTMLAPI HTMLengine* HMBBAPI CreateHtml()
{
	return (HTMLengine *)malloc(sizeof(HTMLengine));
}
HTMLAPI void HMBBAPI engineFree(HTMLengine* e, GumboOutput *engine);
HTMLAPI GumboOutput* HMBBAPI LoadHtml(HTMLengine* html,const tchar_t* HTML,size_t len)
{
	//throw "sss";
	//加载解析一个html字符串is_str_utf8(HTML)
	if (0)
	{
		return gumbo_parse((const char*)HTML);
	}
	else {
		//MessageBoxA(0, to_string(strlen(htmla)).c_str(), _T("数据len："), 0);
		return html->HTML_Create(litehtml_to_utf8(HTML));
	}
}
HTMLAPI GumboNode* HMBBAPI GetRootNode(HTMLengine* html, GumboOutput *engine)
{
	return html->HTML_GetRootNode(engine);
}
HTMLAPI LPVOID HMBBAPI Find(HTMLengine *html, GumboNode* gum, char* path, void* JsonCode)
{
	char* path1 = (char*)malloc(strlen(path) + 1);
	strcpy(path1, path);
	Json::Value jsondata;
	GumboNodes *nodes = new GumboNodes;
	vector<GumboNode*> asd = CLHQ(html, gum, path1, NULL, &jsondata);
	//MessageBoxA(0, to_string(asd.size()).c_str(), _T("数据量："), 0);
	//MessageBoxA(0, to_string(strlen((char*)JsonCode)).c_str(), _T("指针长度："), 0);
	nodes->assign(asd.begin(), asd.end());
	free(path1);
	string strdata = jsondata.toStyledString();
	jsondata.clear();
	strcpy_s((char*)JsonCode, strdata.size() + 1, strdata.data());
	return nodes;
}
HTMLAPI LPVOID HMBBAPI Find1(HTMLengine *html, GumboNode* gum, vector<GumboNode*> *nodes, char* path)
{
	GumboNodes *nodess = new GumboNodes;
	vector<GumboNode*> asd = CLHQ(html, gum, path, nodes);
	nodess->assign(asd.begin(), asd.end());
	return nodess;
}
HTMLAPI void HMBBAPI engineFree(HTMLengine* e, GumboOutput *engine)
{
	e->HTML_Destory(engine);
}
HTMLAPI void HMBBAPI engineClose(HTMLengine* e)
{
	free(e);
}
HTMLAPI GumboNode* HMBBAPI GetNode(vector<GumboNode*> &asd, size_t i)
{
	if (i >= asd.size())return NULL;
	return asd[i];
}
HTMLAPI int HMBBAPI GetNodeSize(vector<GumboNode*> &asd)
{
	if (asd.size() <= 0)return 0;
	return asd.size();
}
HTMLAPI void HMBBAPI GetNodeFree(vector<GumboNode*> *asd)
{
	asd->clear();
	delete asd;
}
HTMLAPI const size_t HMBBAPI GetNodeText(HTMLengine* html, GumboNode* htmlcode, BOOL isU_G, void* text)
{
	try
	{
		std::string TEXT;
		size_t n = html->HTMLNode_GetChildCount(htmlcode);
		for (size_t i = 0; i < n; i++)
		{
			GumboNode* hcode = static_cast<GumboNode*>(htmlcode->v.element.children.data[i]);
			if (hcode->type == GUMBO_NODE_TEXT)
			{
				TEXT.append(hcode->v.text.text);
			}
		}
		string data = U2G(TEXT.c_str());
		strcpy_s((char*)text, data.size() + 1, data.c_str());
		return data.size();
	}
	catch (const std::exception&)
	{
		return 0;
	}

}
HTMLAPI const int HMBBAPI GetNodeHtml(GumboNode* htmlcode, BOOL isU_G, void* text)
{
	try
	{
		std::string no(htmlcode->v.element.original_tag.data, htmlcode->v.element.original_end_tag.data - htmlcode->v.element.original_tag.data + htmlcode->v.element.original_end_tag.length);
		string data = U2G(no.c_str());
		strcpy_s((char*)text, data.size() + 1, data.c_str());
		//memcpy(text, data.data(), data.size());//data.length() + 1
		return data.size();
	}
	catch (const std::exception&)
	{
		return 0;
	}
}
HTMLAPI const char* HMBBAPI GetAttr(GumboNode* htmlcode, const char* name)
{
	try
	{
		GumboAttribute* guma=gumbo_get_attribute(&htmlcode->v.element.attributes, name);
		return (guma==NULL)?"": guma->value;
	}
	catch (const std::exception&)
	{
		return "\0";
	}
}

#include <algorithm>
#include <fstream>
bool ReadFileTowstring(wstring szfile, wstring &content);
int main(int argc, char* argv[])
{
	
	//_CrtSetBreakAlloc(169876);
	const char* filename = "IGC_DropManager.xml";
	std::wstring data;
	ReadFileTowstring(L"IGC_DropManager.xml", data);
	/*std::ifstream in(filename, std::ios::in | std::ios::binary);
	if (!in) {
		std::cout << "File " << filename << " not found!\n";
	}

	std::string contents;
	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	in.close();*/
	//.serial-wrap cf>div>.left - wrap>div:find('data-l1', '10')>div>ul>li:find('data-l2', '3')>div>p
	sds patha = sdsnew("Item");//a:find('class','green')

	HTMLengine *html = CreateHtml();
	//加载解析一个html字符串
	
	GumboOutput *engine = LoadHtml(html, data.c_str(), data.size()); //html->HTML_Create(HTML);
													 //获取html根节点
	GumboNode* gum = html->HTML_GetRootNode(engine);
	char *text = (char*)malloc(888888);
	GetNodeHtml(gum,FALSE, text);
//	std::string no(gum->v.element.original_tag.data, gum->v.element.original_end_tag.data - gum->v.element.original_tag.data + gum->v.element.original_end_tag.length);
	//根据自定义css选择器函数解析一个节点或者节点数组
	//.serial-wrap cf>div>.left-wrap>div:find('data-l1','10')>div>ul>li:find('data-l2','3')>div>p
	//div:find('data-l1','10')>div>ul>li:find('data-l2','3')>div>p

	cout << "表达式: " << patha << "\n";
	Json::Value jsondata;

	//FindJson(html, gum, path, dada);

	start = clock();
	char kk[10000];
	
	GumboNodes* HTMLnode = (GumboNodes*)Find(html, gum, patha, kk);
	//GetNodeFree(HTMLnode);
	finish = clock();
	totaltime = (double)(finish - start) / CLOCKS_PER_SEC;

	cout << "\n递归次数：" << n << endl;
	cout << "\n解析出来节点数量：" << HTMLnode->size() << endl;
	cout << "\n此程序的运行时间为" << totaltime << "秒！" << endl;
	//Source.def
	//返回节点数组
	//切记不能销毁引擎之后再去读节点 会爆炸的
	/*cout << jsondata.toStyledString().size() << endl;
	cout << jsondata.toStyledString() << endl;
	for (size_t i = 0; i < HTMLnode.size(); i++)
	{
	//cout << HTMLnode[i]->v.element.children.length << "\n";
	//std::string no(htmlcode->v.element.original_tag.data, htmlcode->v.element.original_end_tag.data- htmlcode->v.element.original_tag.data+ htmlcode->v.element.original_end_tag.length);
	//std::string eo(, HTMLnode[i]->v.element.original_end_tag.length);
	//std::cout << U2G(no.c_str()) << std::endl;
	cout << "结果:" << U2G(html->HTMLNode_GetText(HTMLnode[i]).c_str()) << "\n";
	//cout << "结果:" << no.c_str() << "\n";
	}*/
	//销毁引擎
	engineFree(html, engine);
	free(html);
	//html->HTML_Destory(engine);
	_CrtDumpMemoryLeaks();

	system("pause");
	return 0;
}
//读取filename文件的内容到wstring
bool ReadFileTowstring(wstring szfile, wstring &content)
{
	//存储读取的wstring
	content.clear();
	wchar_t linex[4096];

	FILE * file;
	//格式化filename为wchar_t *
	//wchar_t *wc = new wchar_t[filename.length()];
	//wchar_t *wmode = wc;
	//swprintf(wc, L"%S", filename);

	//读取utf-8编码的文件
	file = _wfopen(szfile.c_str(), L"rt+,ccs=UTF-8");
	locale loc("");
	wcout.imbue(loc);
	//while (!feof(file))
	while (fgetws(linex, 4096, file))
	{
		//fgetws(linex,4096,file);
		//wstring linew = linex;
		//content+=linew;
		content += wstring(linex);
	}
	fclose(file);
	return true;
}

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头中排除极少使用的资料
#include <Windows.h>
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}