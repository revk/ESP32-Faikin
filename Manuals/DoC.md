
# Compliance statement

The Faikin hardware is an open source PCB design in this repository, and so you can make it yourself. However, we (Andrews & Arnold) do have the boards made and imported and we sell them.

# Not a product

*CE/UKCA*: What we sell is very specifically an "assembled PCB", a "component", and not itself a "finished product". It has no case, leads, or power supply. You need to snap out of the PCB panel. Whilst the system on chip has a CE mark in itself, we don't make any CE/UKCA conformance statement for this as a product as it is not a product. If someone sells/installs air conditioning units with these modules then the whole thing is a product and they need to ensure compliance with legislation.

*Other regulations*: It is unclear if the UK law: [The Product Security and Telecommunications Infrastructure Act 2022](https://www.legislation.gov.uk/ukpga/2022/46/contents), and [The Product Security and Telecommunications Infrastructure (Security Requirements for Relevant Connectable Products) Regulations 2023](https://www.legislation.gov.uk/uksi/2023/1007/contents/made) apply here or not. As we say, it is not a "finished product". However, just in case it does apply, this statement is an attempt to meet the requirements.

As the Faikin hardware can be made and distributed by anyone, obviously, if they make it, they have to consider compliance, so this statement only applies to cases where we (Andrews & Arnold) have made the Faikin hardware, or distribute it.

# The product (if somehow it is deemed a product)

As the Faikin hardware is a general purpose module / circuit, it only really makes sense to make any statement about it "as supplied", which is with the Faikin software pre-loaded for your convenience.

If you choose to load any other software, or even build a variation of the Faikin software with your own (or someone else's) changes, then it is no longer "as supplied", and you now have a different "product" (if it is a "product") that you have made, so compliance is then up to you.

It may be that the regulations also apply to the Faikin software even when loaded on your own hardware, this too is not entirely clear, or entirely clear with country's laws would apply, especially as that typically means you "build" the software yourself (so is it your software now, built in your country, from a regulatory point of view?).

The code is also in a fork built for ESP8266, and may be in any number of other forks. Obviously I cannot make any statement for those forks, or clones, or copies based on this code.

# Type/batch

So, the "product" (if it is one) is the software on this repository, all batches and builds, on the Faikin hardware made and supplied by Andrews & Arnold, and "as supplied", or as updated from this repository or OTA server.

# Who

The software is made by Adrian Kennard, and other contributors. However, I can only speak for me (Adrian Kennard), I have no idea if any and all contributors also need to make a compliance statement, sorry.

When Andrews & Arnold manufactures and distributes boards, its name is "Andrews & Arnold Ltd", UK company 3342760, and its registered office address is Enterprise Court, Downmill Road, Bracknell, Berkshire, RG12 1QS.

# Statement made by manufacturer

The hardware covered here is made by/for Andrews & Arnold. This statement is prepared by Adrian Kennard, director Andrews & Arnold, on behalf of the "manufacturer".

# Compliance

It is the opinion of the manufacturer that we have complied with Schedule 1 & 2, and 5.1-1 of ETSI EN 303 645. Specifically that where passwords are used, they are user specified, and that we have a vulnerability disclosure policy, etc.

- Normal web access to controls for your aircon on the Faikin have no passwords.
- Access to settings can have no password, or can have a user specified password.

# Vulnerability disclosure policy

Please raise any security issue on this GitHub respository. I would expect to reply promptly but at the very least within one month, and will provide any status updates on the GitHub issue. As this is free software with no contract or warranty, saying I'll reply in a month is not a contractual statement. In practice I usually reply within hours.

Please note that not all *issues* are *security issues*, and this policy applies only to "security issues".

# Support / updates

Updates are made available via this repository. I also provide an OTA server, with beta releases. Some older code did not auto update but allowed manual updates, however, these days, auto updates are enabled by default.

The ultimate place to get the latest code, with any security updates, is this repository.

# You can help

Not something the legislation seems to consider, but I am also happy to consider merge requests that address a vulnerability.

# Support period

I plan to continue to support this product until I get bored, or drop dead, whichever is sooner. I expect that to be many more years.

# Issued

The place this was issued? Well, err, this repository, issued on 2024-03-14 (Pi day). Though check the GPG signature for definitive date/time.

Failing that, it was issued from my computer, in Wales.

# Signed

See [signature](DoC.md.asc).
