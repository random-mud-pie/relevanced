# JVM Clients

## Installation

Relevanced has a Java client which can also be used from any other JVM language.  It's hosted on Sonatype OSS.

To add it to a Maven project, add the following dependency to your `pom.xml` file:
```xml
<dependency>
  <groupId>org.relevanced</groupId>
  <artifactId>client</artifactId>
  <version>0.9.2-SNAPSHOT</version>
</dependency>
```

To add it to an SBT project (Java or Scala), add the same dependency to the `libraryDependencies` in `build.sbt`.  You will also need to add the Sonatype OSS Snapshots repository to your `resolvers`:

```scala
resolvers +=
  "Sonatype OSS Snapshots" at "https://oss.sonatype.org/content/repositories/snapshots"

libraryDependencies ++= Seq(
   "org.relevanced" % "client" % "0.9.2-SNAPSHOT"
)

```
## API

The Java client API has the same commands described in the [language-agnostic command overview](../commands.md), and follows the same `camelCase` naming convention.  In the case of the synchronous client (`RelevancedBlockingClient`), the semantics are exactly identical to the description in that document.

Basic use of the blocking client:

```java
package org.relevanced.example;
import java.lang.System;
import org.apache.thrift.TException;
import org.relevanced.client.RelevancedBlockingClient;

public class Main {
    public static void main(String [] args) {
        try {
            RelevancedBlockingClient client = RelevancedBlockingClient.connect(
                "localhost", 8097
            );
            client.createCentroid("centroid-id");
            client.createDocumentWithId("doc-id", "This is some relevant text");
            client.addDocumentToCentroid("centroid-id", "doc-id");
            client.joinCentroid("centroid-id");
            double textSimilarity = client.getTextSimilarity("centroid-id",
                "This is a potentially relevant document."
            );
            System.out.println("similarity: " + textSimilarity);
        } catch (TException err) {
            err.printStackTrace();
        }
    }
}
```


Also see the Java [binary classifier](../examples/java-binary-classifier.md) and [multi-class classifier](../examples/java-multiclass-classifier.md) examples for more practical use.

There is also a [Scala version](../examples/scala-binary-classifier.md) of the binary classifier example.