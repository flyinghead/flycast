#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#include "types.h"	

int msgboxf(const wchar* text,unsigned int type,...)
{
	va_list args;

	wchar temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);

	puts(temp);
	return 0;
}


namespace hello_world
{
class HelloWorldInstance : public pp::Instance {
 public:
  explicit HelloWorldInstance(PP_Instance instance) : pp::Instance(instance) {
    printf("HelloWorldInstance.\n");
  }
  virtual ~HelloWorldInstance() {}

  /// Called by the browser to handle the postMessage() call in Javascript.
  /// Detects which method is being called from the message contents, and
  /// calls the appropriate function.  Posts the result back to the browser
  /// asynchronously.
  /// @param[in] var_message The message posted by the browser.  The possible
  ///     messages are 'fortyTwo' and 'reverseText:Hello World'.  Note that
  ///     the 'reverseText' form contains the string to reverse following a ':'
  ///     separator.
  virtual void HandleMessage(const pp::Var& var_message);
};

void HelloWorldInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string()) {
    return;
  }

  pp::Var return_var;

  // Post the return result back to the browser.  Note that HandleMessage() is
  // always called on the main thread, so it's OK to post the return message
  // directly from here.  The return post is asynhronous: PostMessage returns
  // immediately.
  PostMessage(return_var);
}

/// The Module class.  The browser calls the CreateInstance() method to create
/// an instance of your NaCl module on the web page.  The browser creates a new
/// instance for each <embed> tag with
/// <code>type="application/x-nacl"</code>.
class HelloWorldModule : public pp::Module {
 public:
  HelloWorldModule() : pp::Module() {
    printf("Got here.\n");
  }
  virtual ~HelloWorldModule() {}

  /// Create and return a HelloWorldInstance object.
  /// @param[in] instance a handle to a plug-in instance.
  /// @return a newly created HelloWorldInstance.
  /// @note The browser is responsible for calling @a delete when done.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new HelloWorldInstance(instance);
  }
};
}  // namespace hello_world


namespace pp {
/// Factory function called by the browser when the module is first loaded.
/// The browser keeps a singleton of this module.  It calls the
/// CreateInstance() method on the object you return to make instances.  There
/// is one instance per <embed> tag on the page.  This is the main binding
/// point for your NaCl module with the browser.
/// @return new HelloWorldModule.
/// @note The browser is responsible for deleting returned @a Module.
Module* CreateModule() {
  return new hello_world::HelloWorldModule();
}
}  // namespace pp