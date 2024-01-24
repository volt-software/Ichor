# Introspection Example

This example showcases Ichor's capability introspect services and properties, without knowing their type directly. Each service contains metadata about its Id/Guid, whether it is started, what its properties are and so on. 

Introspecting allows for monitoring and controlling individual services. In this example, all services are iterated and their metadata printed to stdout. It also shows how to change logger settings that belong to a specific service. 

Example output:

```text
Service 1:Ichor::IEventQueue, guid 93319e7b-319e-45d0-b601-755ce135720b priority 1000 state INSTALLED
	Interfaces:
		Interface Ichor::IEventQueue hash 2671418589834563989
	Properties:
	Dependencies:
	Used by:
		Dependant 5:IntrospectionService
		Dependant 7:Ichor::TimerFactory
	Trackers:

Service 2:Ichor::DependencyManager, guid 13b34101-b4f9-4987-a58c-cce9e401b4ae priority 1000 state INSTALLED
	Interfaces:
		Interface Ichor::DependencyManager hash 6048060061124314661
	Properties:
	Dependencies:
	Used by:
		Dependant 5:IntrospectionService
	Trackers:

Service 3:Ichor::LoggerFactory<Ichor::CoutLogger>, guid ad5bd7a1-f94d-41eb-bd9e-756ae81a978b priority 1000 state ACTIVE
	Interfaces:
		Interface Ichor::ILoggerFactory hash 4064173856622543226
	Properties:
		Property DefaultLogLevel value LOG_INFO size 8 type Ichor::LogLevel
	Dependencies:
		Dependency Ichor::IFrameworkLogger flags NONE satisfied 0
	Used by:
	Trackers:
		Tracker for interface Ichor::ILogger hash 13978885023924282811

Service 4:Ichor::TimerFactoryFactory, guid 01198fe5-2751-4d56-82a4-d2e25a45e3e1 priority 1000 state INJECTING
	Interfaces:
	Properties:
	Dependencies:
	Used by:
	Trackers:
		Tracker for interface Ichor::ITimerFactory hash 8128936943696194044

Service 5:IntrospectionService, guid ad91d65c-975d-4429-8137-719b0c8791ae priority 1000 state STARTING
	Interfaces:
	Properties:
	Dependencies:
		Dependency Ichor::ITimerFactory flags REQUIRED satisfied 1
		Dependency Ichor::ILogger flags REQUIRED satisfied 1
		Dependency Ichor::DependencyManager flags REQUIRED satisfied 1
		Dependency Ichor::IEventQueue flags REQUIRED satisfied 1
		Dependency Ichor::IService flags REQUIRED satisfied 1
	Used by:
	Trackers:

Service 6:Ichor::CoutLogger, guid 71bcde8e-b686-4b86-84f9-d3c3765278d3 priority 100 state ACTIVE
	Interfaces:
		Interface Ichor::ILogger hash 13978885023924282811
	Properties:
		Property Filter value  size 8 type Ichor::Filter
		Property LogLevel value LOG_INFO size 8 type Ichor::LogLevel
	Dependencies:
	Used by:
		Dependant 5:IntrospectionService
	Trackers:

Service 7:Ichor::TimerFactory, guid 5beeaf6a-12a3-49bb-a440-a432e7b0c01f priority 100 state ACTIVE
	Interfaces:
		Interface Ichor::ITimerFactory hash 8128936943696194044
	Properties:
		Property requestingSvcId value 5 size 8 type unsigned long
	Dependencies:
		Dependency Ichor::IEventQueue flags REQUIRED satisfied 1
	Used by:
		Dependant 5:IntrospectionService
	Trackers:
```
