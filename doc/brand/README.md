# iCaNova Brand Integration & Repository Rebranding

## Cursor Implementation Specification

**Project:** iCaNova
**Parent company:** ASIEDU DEVELOPMENT HUB (ADH)
**Document:** Brand Integration and Repository Migration
**Version:** 1.0
**Implementation environment:** Shared GitHub Codespace containing two repositories
**Status:** Approved direction for implementation, subject to the safeguards below.

---

# 1. OBJECTIVE

We are transitioning the existing DSE monitoring project into a broader industrial technology platform under the brand **iCaNova**.

The existing project currently has two repositories:

1. The DSE Gateway repository — hardware firmware and embedded device software.
2. The DSE Web repository — web-based monitoring platform.

Both repositories exist within the same Codespace/workspace.

We want to rebrand the existing system to iCaNova, including:

- Repository names.
- Project names and descriptions.
- Application branding.
- Product identity.
- Documentation.
- README files.
- Package metadata where appropriate.
- Website branding.
- Hardware identity and product terminology.
- Shared project documentation.
- Brand-related configuration and assets.

This must be performed without breaking existing functionality.

**This is a rebranding and identity migration, not a ground-up redevelopment.**

Do not rewrite the existing firmware, backend, frontend architecture, authentication system, communication protocols or infrastructure simply to accommodate the new brand.

---

# 2. BRAND IDENTITY — SOURCE OF TRUTH

The following brand specifications should be treated as the current approved project identity.

## 2.1 Brand name

**iCaNova**

Official capitalization:

`iCaNova`

Pronunciation:

`eye-can-OH-va`

Do not write the brand as:

- Icanova
- ICANOVA
- ICanova
- iCANOVA

Use `iCaNova` in user-facing brand references.

Technical identifiers such as package names, repository names, environment variables and URLs should follow their applicable naming conventions, normally lowercase.

## 2.2 Tagline

**Imagine. Connect. Innovate.**

Use this as the primary brand tagline.

## 2.3 Corporate descriptor

**Intelligent technology for connected industries.**

This explains the company's positioning and should not replace the primary tagline.

## 2.4 Brand positioning

iCaNova is an industrial technology brand developing intelligent hardware and software solutions that connect physical equipment, industrial systems, infrastructure and operational data.

The platform should support a growing ecosystem of:

- Industrial equipment monitoring.
- Energy management.
- Power systems.
- Generators.
- Inverters.
- Solar installations.
- PLCs.
- Sensors.
- Pumps.
- HVAC systems.
- Industrial automation.
- Edge computing.
- Cloud monitoring.
- Device connectivity.
- Operational intelligence.

Do not position iCaNova as exclusively a Modbus company, DSE monitoring product, ESP32 project or electricity-monitoring application.

The current DSE monitoring functionality is one product capability within a broader industrial technology platform.

## 2.5 Brand personality

The five defining characteristics are:

1. Precise.
2. Intelligent.
3. Dependable.
4. Progressive.
5. Confident.

## 2.6 Visual direction

**Industrial Precision + Modern Technology**

The design language must communicate:

- Industrial engineering credibility.
- Professional technology.
- Precision manufacturing.
- Reliability.
- Modern enterprise software.
- Practical innovation.

Avoid generic AI-startup aesthetics.

Specifically avoid:

- Excessive blue gradients.
- Purple/blue AI gradients.
- Neon glowing effects.
- Random circuit-board backgrounds.
- Excessive glassmorphism.
- Holographic visual effects.
- Generic AI-generated technology imagery.
- Decorative hexagons and circuit patterns without functional purpose.

---

# 3. OFFICIAL BRAND COLOUR SYSTEM

Use these colours as the initial brand design tokens.

## 3.1 Primary colours

| Colour             | HEX       | Purpose                    |
| ------------------ | --------- | -------------------------- |
| iCaNova Graphite   | `#111820` | Primary dark colour        |
| iCaNova Warm White | `#F2F1ED` | Primary light colour       |
| iCaNova Deep Navy  | `#263746` | Secondary corporate colour |

## 3.2 Supporting colours

| Colour           | HEX       | Purpose                        |
| ---------------- | --------- | ------------------------------ |
| Titanium Silver  | `#B8BABC` | Metallic and secondary details |
| Light Silver     | `#D8D9D7` | Light surfaces and dividers    |
| Industrial Steel | `#637B83` | Technical supporting colour    |
| Muted Copper     | `#A8754D` | Restrained premium accent      |

## 3.3 Dark environment

Use:

- Graphite as the dominant background.
- Deep Navy for secondary surfaces.
- Titanium for restrained visual details.
- Warm White for readable text.
- Copper for occasional accents.

## 3.4 Light environment

Use:

- Warm White as the dominant background.
- Deep Navy for navigation and important interface elements.
- Graphite for primary text.
- Light Silver for borders and dividers.
- Steel and Copper for restrained accents.

## 3.5 Colour rules

1. Do not introduce additional corporate colours without a clear reason.
2. Do not use gradients as the default background treatment.
3. Maintain accessible text contrast.
4. Do not use Copper as the dominant brand colour.
5. Avoid metallic gradients in normal application components.
6. Keep status colours separate from corporate identity colours.

## 3.6 Functional status colours

These are interface semantics, not additional brand colours.

| Status            | HEX       |
| ----------------- | --------- |
| Success / Online  | `#24845B` |
| Warning           | `#D18A26` |
| Critical / Error  | `#C64747` |
| Information       | `#477CA8` |
| Offline / Unknown | `#707780` |

Always accompany status colours with text or icons.

---

# 4. TYPOGRAPHY SYSTEM

Use the following font hierarchy.

| Usage                   | Typeface      |
| ----------------------- | ------------- |
| Brand headings          | Manrope       |
| Application UI and body | Inter         |
| Technical data          | IBM Plex Mono |

Recommended weights:

- Manrope: 500, 600, 700.
- Inter: 400, 500, 600.
- IBM Plex Mono: 400, 500.

Use appropriate font fallbacks.

Do not assume external font downloads will always be available. Preserve the application's ability to render using system fallbacks.

Suggested CSS:

```css
:root {
  --font-display: "Manrope", sans-serif;
  --font-body: "Inter", sans-serif;
  --font-mono: "IBM Plex Mono", monospace;
}
```

For firmware and embedded documentation, prioritize readable technical typography rather than introducing unnecessary font dependencies.

---

# 5. REPOSITORY RENAMING

We currently have two repositories in the same Codespace.

The proposed new repository names are:

| Existing project | New repository name      |
| ---------------- | ------------------------ |
| DSE Gateway      | `icanova-edge-gateway`   |
| DSE Web          | `icanova-monitoring-web` |

These names are the proposed target names.

## 5.1 Important: inspect before changing

Before performing any rename:

1. Inspect the Codespace root.
2. Identify both repository directories.
3. Inspect their Git remotes.
4. Identify the active branches.
5. Identify uncommitted changes.
6. Identify whether the repositories have independent `.git` directories.
7. Determine whether the workspace is a multi-root workspace or a shared parent directory.
8. Identify any submodules, worktrees or cross-repository dependencies.
9. Record the existing GitHub repository URLs.
10. Identify CI/CD configurations that reference the old repository names.

Do not assume the directory names and remote repository names are identical.

Do not move, merge or nest Git repositories simply to make their names look consistent.

## 5.2 GitHub repository rename

If the existing repositories are hosted on GitHub:

- Rename the gateway repository to `icanova-edge-gateway`.
- Rename the web repository to `icanova-monitoring-web`.

Only perform a remote rename after verifying the correct repository owner, current repository name and available authorization.

Preserve:

- Commit history.
- Issues.
- Pull requests.
- Branches.
- Tags.
- Release history.
- Repository permissions.
- GitHub Actions configuration.
- Existing project associations where possible.

Do not delete and recreate repositories as a substitute for renaming.

After renaming, update each repository's `origin` remote to its new canonical URL.

Verify both remotes with:

```bash
git remote -v
```

Do not assume GitHub's redirects will be sufficient for permanent CI/CD integrations.

If the rename cannot be performed automatically, document the exact manual operation required and continue only with safe local changes.

## 5.3 Local directory names

Where appropriate, rename the local repository directories to:

```text
icanova-edge-gateway/
icanova-monitoring-web/
```

Preserve the existing shared Codespace arrangement.

Update the workspace configuration, multi-root workspace references, scripts and local development instructions to use the new directory names.

Do not rename the parent Codespace or change the workspace structure unnecessarily.

---

# 6. PROJECT ARCHITECTURE AND BRAND RELATIONSHIP

The two repositories represent separate parts of one product ecosystem.

The architecture should conceptually be represented as:

```text
                  ASIEDU DEVELOPMENT HUB
                            |
                          iCaNova
                            |
                INDUSTRIAL TECHNOLOGY PLATFORM
                            |
             +--------------+--------------+
             |                             |
       EDGE HARDWARE                  WEB PLATFORM
             |                             |
    icanova-edge-gateway        icanova-monitoring-web
             |                             |
    Embedded firmware             Monitoring interface
    Device communication          Device management
    RS485 interfaces               Data visualization
    Modbus RTU                     Alerts
    Cellular connectivity          Authentication
    Local processing               Operations
             |                             |
             +-------------+---------------+
                           |
                    CONNECTED SYSTEMS
```

This is a logical brand architecture, not an instruction to merge the repositories.

Keep the hardware and web repositories independently maintainable and deployable.

---

# 7. GATEWAY REPOSITORY REBRANDING

Target repository:

`icanova-edge-gateway`

The gateway repository is responsible for the physical industrial device and embedded firmware.

## 7.1 Product identity

The device should be identified as an iCaNova industrial edge gateway.

Suggested generic product terminology:

**iCaNova Edge Gateway**

Do not permanently assign a commercial model number unless an existing model specification or approved product specification exists.

## 7.2 Firmware branding

Review and update appropriate user-facing references in:

- README.
- Firmware documentation.
- Device information screens, if present.
- Serial startup messages.
- Firmware release notes.
- Product identification documentation.
- Build and release metadata.
- Device configuration documentation.
- Hardware design documentation.
- Product specifications.

Example startu message:

```text
=====================================
             iCaNova
       iCaNova Edge Gateway
=====================================
Imagine. Connect. Innovate.
Family: iCaNova Edge

Board: ESP32-C3 SuperMini
Device ID: <DEVICE_ID>
Communication: RS485 / Modbus RTU
```

The example matches current CLI branding. There is no dedicated firmware version string yet — do not invent one in production metadata.

## 7.3 Hardware identity

The physical product branding should follow:

- Matte graphite black enclosure.
- Minimal silver or white markings.
- Professional industrial labels.
- Clear terminal identification.
- Consistent model and serial-number layout.
- Restrained metallic details.

The physical design should look like professional industrial equipment rather than a hobbyist development board.

Do not alter the electrical design, PCB, terminal block count, ESP32-C3 architecture, SIM-card mechanism, RS485 protection or other hardware specifications during a branding-only migration.

Do not modify electrical or PCB design files merely to rename the project.

## 7.4 Embedded identifiers

Be extremely careful with firmware identifiers.

Do not automatically rename:

- Modbus register addresses.
- Modbus function codes.
- Device protocol identifiers.
- UART configuration identifiers.
- RS485 pin mappings.
- GPIO definitions.
- Flash partitions.
- NVS keys.
- Persistent storage keys.
- OTA partition labels.
- Bootloader configuration.
- Hardware revision identifiers.
- Existing device IDs.
- Communication message structures.
- Firmware update compatibility identifiers.

These may be part of compatibility contracts.

Only change identifiers that are genuinely brand-facing or have a safe migration strategy.

Where an existing identifier must remain unchanged for compatibility, document why.

## 7.5 Gateway README

Create or update the README with:

1. iCaNova product introduction.
2. Gateway purpose.
3. Hardware architecture.
4. Supported communication interfaces.
5. Modbus RTU functionality.
6. RS485 architecture.
7. Cellular connectivity, where implemented.
8. Firmware architecture.
9. Hardware requirements.
10. Build instructions.
11. Flashing instructions.
12. Configuration instructions.
13. Debugging and serial monitoring.
14. Testing procedures.
15. Firmware release procedure.
16. Repository structure.

Clearly distinguish implemented capabilities from planned capabilities.

---

# 8. WEB REPOSITORY REBRANDING

Target repository:

`icanova-monitoring-web`

This repository contains the existing web monitoring platform.

## 8.1 Application identity

The application should identify itself as:

**iCaNova**

Suggested application description:

**Industrial Equipment Monitoring and Management Platform**

The description may be refined according to the actual capabilities currently implemented.

## 8.2 User-facing brand changes

Inspect and update:

- Application name.
- Browser title.
- Page metadata.
- Favicon references.
- Login page branding.
- Navigation brand text.
- Sidebar branding.
- Footer.
- Loading screens.
- Error pages.
- Empty states.
- Notifications.
- Email templates.
- PDF exports.
- Reports.
- Documentation.
- Application manifest.
- PWA metadata, if applicable.

Use the new name consistently.

Do not change authentication providers, callback logic, organization permissions, user permissions or access-control policies as part of this rebranding.

## 8.3 Existing UI/UX preservation

IMPORTANT:

**The full UI/UX redesign is a separate future phase.**

For this implementation, perform a controlled brand migration only.

Permitted changes:

- Replace old DSE project branding with iCaNova.
- Replace appropriate product names.
- Introduce the approved colour tokens.
- Update typography.
- Update basic visual styling.
- Update favicon and brand asset references.
- Update browser and application metadata.
- Update existing layouts only where necessary to accommodate the branding.

Do not:

- Rebuild the dashboard.
- Redesign navigation.
- Change page information architecture.
- Remove existing components.
- Replace working chart libraries.
- Rewrite device-management workflows.
- Change device onboarding.
- Change API contracts.
- Change authentication flows.
- Modify database schemas.
- Change tenant or organization logic.
- Change backend permissions.
- Remove existing monitoring functionality.

Keep existing UX behavior and information architecture intact.

## 8.4 Dark and light themes

The web application should support the brand's dark and light environments where the existing architecture permits.

If the current application already supports themes:

- Preserve theme switching.
- Update both theme token sets.
- Ensure readable text and borders.
- Test all important components.

If it does not support themes:

- Do not introduce a complex new theme architecture solely for this migration.
- Apply the appropriate existing theme.
- Document the future dark/light theme work separately.

---

# 9. SHARED BRAND DESIGN TOKENS

Where both repositories need shared brand values, establish a consistent source of truth.

For the web repository, use the existing design-token mechanism.

For the firmware repository, document the physical colour and typography specifications rather than introducing unnecessary CSS or web dependencies.

Example web design tokens:

```css
:root {
  /* Brand */
  --brand-name: "iCaNova";

  /* Primary */
  --color-graphite: #111820;
  --color-warm-white: #f2f1ed;
  --color-deep-navy: #263746;

  /* Supporting */
  --color-titanium: #b8babc;
  --color-light-silver: #d8d9d7;
  --color-industrial-steel: #637b83;
  --color-muted-copper: #a8754d;

  /* Functional */
  --color-success: #24845b;
  --color-warning: #d18a26;
  --color-danger: #c64747;
  --color-info: #477ca8;
  --color-offline: #707780;

  /* Typography */
  --font-display: "Manrope", sans-serif;
  --font-body: "Inter", sans-serif;
  --font-mono: "IBM Plex Mono", monospace;

  /* Borders */
  --radius-sm: 4px;
  --radius-md: 8px;
  --radius-lg: 12px;
}
```

This is a conceptual example.

Adapt it to the existing framework and CSS architecture.

Do not introduce an unnecessary new styling system if one already exists.

If the repositories need a shared package in the future, propose it separately. Do not create a third repository or package solely for the branding migration.

---

# 10. BRAND ASSETS AND DIRECTORY STRUCTURE

Inspect the existing asset organization before creating new directories.

Where suitable, establish a structure similar to:

```text
brand/
├── README.md
├── brand-specification.md
├── colours.md
├── typography.md
├── design-principles.md
├── product-identity.md
└── assets/
    ├── logo/
    │   ├── primary/
    │   ├── monochrome/
    │   └── icon/
    ├── icons/
    ├── patterns/
    └── product/
```

Do not invent or create a final logo.

The logo is intentionally on hold.

Until the final logo is approved:

- Use a clean text-based iCaNova wordmark.
- Avoid invented logo symbols.
- Do not use generic AI icons.
- Keep brand assets easy to replace later.
- Do not make the temporary text wordmark a permanent technical dependency.

If the repositories are better served by a single shared brand document, maintain that document in the most appropriate existing project location and reference it from the other repository.

Avoid unnecessary duplication.

---

# 11. REPOSITORY DOCUMENTATION

Update both repositories.

## Gateway repository

README title:

`# iCaNova Edge Gateway`

Description:

`Embedded industrial edge gateway for connecting and monitoring industrial equipment through supported communication interfaces.`

## Web repository

README title:

`# iCaNova Monitoring Platform`

Description:

`Web-based platform for industrial equipment monitoring, device management, operational visibility and connected system management.`

Descriptions must reflect actual implementation. Do not advertise unimplemented capabilities as finished functionality.

Both repositories should include a reference to the parent organization:

`A technology platform by ASIEDU DEVELOPMENT HUB (ADH).`

Use the parent-company reference where appropriate, not in every UI element.

---

# 12. PACKAGE AND TECHNICAL METADATA

Inspect all project metadata.

Potential locations include:

- `package.json`.
- `Cargo.toml`, if applicable.
- PlatformIO configuration.
- CMake configuration.
- ESP-IDF project configuration.
- Application manifests.
- Docker configurations.
- Compose files.
- Workspace configuration.
- Build scripts.
- CI/CD workflows.
- Release workflows.
- Documentation.
- Environment examples.

Update brand-facing project names where safe.

For example:

```json
{
  "name": "icanova-monitoring-web"
}
```

This is only an example. Preserve the current package structure, versioning and dependencies.

Do not change package names if doing so would break imports, deployment configuration or published package compatibility without an explicit migration.

For firmware project identifiers, check the effects of changing the project name before modifying build targets or binary output names.

---

# 13. CROSS-REPOSITORY REFERENCES AND DEPLOYMENT

Since both repositories exist in the same Codespace, inspect all cross-repository dependencies.

Search for old repository references in:

- Git remotes.
- Workspace configuration.
- CI/CD workflows.
- Deployment settings.
- GitHub Actions.
- Docker build contexts.
- Documentation.
- Scripts.
- Submodules.
- Package references.
- Release automation.
- Environment configuration.
- Webhooks.
- External deployment services.

Update references only when the actual target has changed.

Preserve:

- Existing deployment environments.
- Environment variable names.
- Secret names.
- Authentication configuration.
- Production URLs.
- API endpoints.
- Database references.
- Device communication settings.
- Firmware update URLs.
- Cloud service configuration.

Do not rotate secrets, change credentials or modify production endpoints as part of this task.

If a remote repository rename affects a deployment integration, document and resolve it using the existing deployment provider's supported procedure.

Do not assume that changing a GitHub repository name automatically updates every external integration.

---

# 14. BRAND NAME MIGRATION RULES

Search both repositories for existing DSE branding.

Use searches for terms such as:

```text
DSE
Deep Sea Electronics
DSE Gateway
DSE Monitoring
dse-gateway
dse-web
```

Review every match before replacing it.

Classify matches into:

1. User-facing brand references.
2. Project metadata.
3. Documentation.
4. Technical identifiers.
5. External vendor references.
6. Historical references.
7. Third-party library references.

Replace the first three where appropriate.

Do not blindly replace every occurrence of DSE.

Deep Sea Electronics is a third-party equipment manufacturer, and DSE devices may still be supported by the platform.

Preserve:

- DSE equipment references.
- Manufacturer identification.
- DSE model names.
- DSE-specific register maps.
- DSE protocol documentation.
- DSE integration logic.
- Third-party attribution.
- Existing device compatibility.

The objective is to change the platform brand, not erase the equipment manufacturer's identity.

---

# 15. SECURITY AND FUNCTIONALITY PRESERVATION

This migration must not weaken the existing platform.

Preserve:

- Authentication.
- Authorization.
- Organization isolation.
- Tenant boundaries.
- User roles.
- Access tokens.
- API security.
- Database permissions.
- Device authentication.
- Firmware update security.
- Network security.
- Existing audit functionality.
- Existing deployment protection.

Do not change:

- Auth0 configuration.
- JWT validation.
- Token audiences.
- Token issuers.
- Organization claims.
- Backend authorization logic.
- Convex or other database permissions.
- API access rules.

Branding changes must not introduce cross-organization data exposure or authentication regressions.

---

# 16. IMPLEMENTATION WORKFLOW

Perform the migration in controlled stages.

## PHASE A — Discovery and audit

Before modifying files:

1. Inspect both repositories.
2. Map the directory structure.
3. Identify technologies and frameworks.
4. Identify current application names.
5. Identify current brand references.
6. Identify shared assets.
7. Identify cross-repository dependencies.
8. Identify uncommitted changes.
9. Identify existing tests.
10. Identify build and deployment commands.

Create a concise migration plan.

Do not start destructive operations.

## PHASE B — Repository identity migration

1. Verify Git repository boundaries.
2. Record current remote URLs.
3. Rename the GitHub repositories where authorized.
4. Update local Git remotes.
5. Rename local directories where appropriate.
6. Update workspace configuration.
7. Update CI/CD references.
8. Verify repository access and branch history.

## PHASE C — Brand foundation

1. Create or update the brand specification.
2. Introduce approved colour tokens.
3. Introduce typography definitions.
4. Establish light and dark colour mappings where supported.
5. Establish temporary text-based brand assets.
6. Document logo replacement requirements.

## PHASE D — Gateway rebranding

1. Update documentation.
2. Update user-facing product identification.
3. Update safe firmware branding strings.
4. Update product descriptions.
5. Update technical documentation.
6. Preserve hardware and protocol behavior.
7. Build the firmware.

## PHASE E — Web rebranding

1. Update application identity.
2. Update page metadata.
3. Update navigation branding.
4. Update existing brand assets.
5. Introduce the brand colour tokens.
6. Update typography.
7. Update login and loading branding.
8. Update documentation and application metadata.
9. Preserve existing UI/UX behavior.
10. Build the application.

## PHASE F — Integration and regression testing

Verify both repositories independently and together.

Run the existing:

- Firmware build.
- Firmware unit tests.
- Firmware integration tests where available.
- Frontend build.
- Frontend lint.
- Frontend tests.
- Backend tests.
- Type checks.
- Security checks where configured.

Do not silently skip failing tests.

Compare failures against the baseline and identify whether any were introduced by the rebranding.

## PHASE G — Final verification

Verify:

- Both repositories retain their commit histories.
- Git remotes point to the correct repositories.
- The shared Codespace still works.
- Cross-repository scripts still work.
- CI/CD references are correct.
- Application branding is consistent.
- Hardware branding references are correct.
- Existing DSE device integrations remain intact.
- Authentication remains functional.
- Existing organization isolation remains intact.
- Existing device communication remains intact.
- Builds complete successfully.

---

# 17. THINGS EXPLICITLY OUT OF SCOPE

Do not perform the following as part of this migration:

1. Complete UI/UX redesign.
2. Dashboard restructuring.
3. Backend architectural redesign.
4. Database schema redesign.
5. Modbus RTU implementation changes.
6. RS485 electrical changes.
7. ESP32-C3 hardware redesign.
8. PCB modifications.
9. SIM-card hardware redesign.
10. Authentication-provider migration.
11. Tenant architecture changes.
12. Cloud infrastructure migration.
13. API contract changes.
14. New device onboarding architecture.
15. Final 3D logo creation.
16. New product feature implementation.
17. Commercial domain purchase.
18. Trademark registration.

These can be handled as separate project phases.

---

# 18. REQUIRED DELIVERABLES

At the end of the implementation, provide:

## Repository deliverables

- Renamed gateway repository, where authorized.
- Renamed web repository, where authorized.
- Updated local repository directories where appropriate.
- Updated Git remotes.
- Updated workspace configuration.
- Updated CI/CD references.

## Branding deliverables

- Brand specification Markdown.
- Updated colour tokens.
- Typography definitions.
- Updated application branding.
- Updated gateway branding references.
- Updated README files.
- Updated documentation.
- Temporary text-based wordmark implementation.

## Technical deliverables

- Firmware build report.
- Web build report.
- Test results.
- Cross-repository verification.
- Deployment configuration audit.
- Security regression check.
- List of files changed.
- List of files deliberately left unchanged.

---

# 19. FINAL IMPLEMENTATION REPORT

At completion, produce a report using this format:

## iCaNova Rebranding Report

### 1. Repository migration

| Item               | Previous           | New                    | Status |
| ------------------ | ------------------ | ---------------------- | ------ |
| Gateway repository | Existing name      | icanova-edge-gateway   |        |
| Web repository     | Existing name      | icanova-monitoring-web |        |
| Local workspace    | Existing structure | Updated structure      |        |
| Git remotes        | Existing URLs      | New URLs               |        |

### 2. Branding implementation

| Component                    | Status |
| ---------------------------- | ------ |
| Brand identity documentation |        |
| Application title            |        |
| Website metadata             |        |
| Navigation branding          |        |
| Colour tokens                |        |
| Typography                   |        |
| Gateway documentation        |        |
| Web documentation            |        |
| Hardware identity references |        |
| Temporary wordmark           |        |

### 3. Technical verification

| Test                            | Result |
| ------------------------------- | ------ |
| Gateway build                   |        |
| Web build                       |        |
| Backend tests                   |        |
| Frontend tests                  |        |
| Type checks                     |        |
| Authentication verification     |        |
| Organization isolation          |        |
| Device communication regression |        |
| Deployment verification         |        |

### 4. Outstanding work

List anything that requires manual intervention, external authorization or future implementation.

### 5. Final assessment

Classify the migration as:

- ACCEPTED — all required work and verification completed.
- PARTIALLY COMPLETED — some work remains.
- BLOCKED — critical issue prevents safe completion.

Never report ACCEPTED if critical functionality or security verification is outstanding.

---

# 20. FINAL INSTRUCTIONS TO CURSOR

You are working on an existing industrial monitoring platform with two independent repositories sharing one Codespace.

Your responsibility is to transition the existing project identity to iCaNova while preserving its existing engineering work.

Think like a senior embedded systems engineer, software architect and product branding engineer.

Prioritize:

1. Existing functionality.
2. Security.
3. Repository integrity.
4. Hardware compatibility.
5. Deployment stability.
6. Brand consistency.
7. Maintainability.

Do not make broad architectural changes to simplify the rebranding.

Do not replace real functionality with mock implementations.

Do not invent completed features.

Do not delete historical documentation simply because it contains the DSE name.

Do not modify production secrets or credentials.

If an operation is destructive, irreversible or requires external authorization, stop and report what is required rather than guessing.

**The final result should be the same working industrial monitoring system, now presented as an iCaNova technology product, with a clean foundation for the future UI/UX redesign and broader product ecosystem.**

---

**END OF IMPLEMENTATION SPECIFICATION**

**Brand:** iCaNova
**Tagline:** Imagine. Connect. Innovate.
**Descriptor:** Intelligent technology for connected industries.
**Parent:** ASIEDU DEVELOPMENT HUB (ADH)
**Gateway repository target:** `icanova-edge-gateway`
**Web repository target:** `icanova-monitoring-web`
**Design direction:** Industrial Precision + Modern Technology
**Visual environments:** Dark + Light
**Logo:** On hold
**Full UI/UX redesign:** Future phase
