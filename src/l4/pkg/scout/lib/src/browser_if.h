#pragma once

class Browser_if
{
public:
  virtual bool go_home() = 0;
  virtual bool go_forward() = 0;
  virtual bool go_backward() = 0;
  virtual bool go_toc() = 0;
  virtual bool go_about() = 0;
  virtual ~Browser_if() {}
};
