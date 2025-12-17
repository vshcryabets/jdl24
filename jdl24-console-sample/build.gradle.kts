plugins {
    java
    application
}

repositories {
    mavenCentral()
}

dependencies {
    implementation(project(":jdl24-library"))
    testImplementation(platform("org.junit:junit-bom:5.10.0"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

application {
    mainClass.set("com.v2soft.Main")
}

tasks.test {
    useJUnitPlatform()
}