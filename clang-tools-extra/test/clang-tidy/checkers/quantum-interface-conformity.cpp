// RUN: %check_clang_tidy %s quantum-interface-conformity %t

class ImplementationNok
{
public:
  void stuff();
  // CHECK-MESSAGES: :[[@LINE-1]]:8: warning: method 'stuff' is not virtual pure in base class 'ImplementationNok' of interface 'IDerivedInterfaceNok' [quantum-interface-conformity]
};

class IBaseInterfaceNok
{
public:
  virtual ~IBaseInterfaceNok() = default;
  virtual void virtualFunc() = 0;
};

class IDerivedInterfaceNok : public IBaseInterfaceNok, public ImplementationNok
{
public:
  virtual void alsoVirtualFunc() = 0;
};

class IOtherInterfaceNok
// CHECK-MESSAGES: :[[@LINE-1]]:7: warning: interface 'IOtherInterfaceNok' has a non virtual destructor [quantum-interface-conformity]
{
public:
  void nonVirtualFunc();
  // CHECK-MESSAGES: :[[@LINE-1]]:8: warning: method 'nonVirtualFunc' is not virtual pure in interface 'IOtherInterfaceNok' [quantum-interface-conformity]
};

class ImplementationOk
{
public:
  void stuff();
};

class IBaseInterfaceOk
{
public:
  virtual ~IBaseInterfaceOk() = default;
  virtual void virtualFunc() = 0;
};

class IDerivedInterfaceOk : public IBaseInterfaceOk
{
public:
  virtual void alsoVirtualFunc() = 0;
};

class IOtherInterfaceOk
{
public:
  virtual ~IOtherInterfaceOk() = default;
  virtual void nonVirtualFunc() = 0;
};
