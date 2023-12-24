# Introspection Example

This example showcases Ichor's capability introspect services and properties, without knowing their type directly. Each service contains metadata about its Id/Guid, whether it is started, what its properties are and so on. 

Introspecting allows for monitoring and controlling individual services. In this example, all services are iterated and their metadata printed to stdout. It also shows how to change logger settings that belong to a specific service. 

Example output:

```text
Service 1:Ichor::IEventQueue, guid 45571cc5-f381-42a3-8e29-529a43d5a164 priority 1000 state INSTALLED
	Properties:
	Dependencies:
	Used by:
		Dependant 7:Ichor::TimerFactory
		Dependant 5:IntrospectionService

Service 2:Ichor::DependencyManager, guid 4bef6ae7-89ac-4ab4-9bed-6b31bf752066 priority 1000 state INSTALLED
	Properties:
	Dependencies:
	Used by:
		Dependant 5:IntrospectionService

Service 3:Ichor::LoggerFactory<Ichor::CoutLogger>, guid 65eb5090-ac98-4f1e-a486-4da196200830 priority 1000 state ACTIVE
	Properties:
		Property DefaultLogLevel value LOG_INFO size 4 type Ichor::LogLevel
	Dependencies:
		Dependency Ichor::IFrameworkLogger required false satisfied 0
	Used by:

Service 4:Ichor::TimerFactoryFactory, guid 3614ba22-7058-4722-bf85-b34370621ef3 priority 1000 state ACTIVE
	Properties:
	Dependencies:
	Used by:

Service 5:IntrospectionService, guid ccc4887f-46a7-438a-b04d-3c7fb84c0a8c priority 1000 state STARTING
	Properties:
	Dependencies:
		Dependency Ichor::ITimerFactory required true satisfied 0
		Dependency Ichor::ILogger required true satisfied 0
		Dependency Ichor::DependencyManager required true satisfied 0
		Dependency Ichor::IEventQueue required true satisfied 0
		Dependency Ichor::IService required true satisfied 0
	Used by:

Service 6:Ichor::CoutLogger, guid 0c3c83e7-55fb-4d10-8c82-705bb2e1d139 priority 1000 state ACTIVE
	Properties:
		Property LogLevel value LOG_INFO size 4 type Ichor::LogLevel
		Property Filter value  size 16 type Ichor::Filter
	Dependencies:
	Used by:
		Dependant 5:IntrospectionService

Service 7:Ichor::TimerFactory, guid 90f45076-a7bb-4d09-8416-c7590654c746 priority 1000 state ACTIVE
	Properties:
		Property requestingSvcId value 5 size 8 type unsigned long
	Dependencies:
		Dependency Ichor::IEventQueue required true satisfied 0
	Used by:
		Dependant 5:IntrospectionService

```
