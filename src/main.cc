#include <iostream>
#include <string>
#include "http/Server.hpp"
#include "http/Render.hpp"

http::Server svr;
int main() {

    svr.get("/", [&]()->std::string{
        return Render::file("./static/dicet-TV/index.html");
        //return "<h1>Rodado de boa o nosso servidor Http!</h1>";
    });
    svr.get("/blog", [&]()->std::string{
        return Render::file("./static/dicet-TV/blog.html");
    });
    svr.get("/about", [&]()->std::string{
        return Render::file("./static/dicet-TV/about.html");
    });
    svr.get("/contact", [&]()->std::string{
        return Render::file("./static/dicet-TV/contact.html");
    });
    svr.get("/products", [&]()->std::string{
        return Render::file("./static/dicet-TV/products.html");
    });

    
    svr.Listen(8080);
    return 0;
}

