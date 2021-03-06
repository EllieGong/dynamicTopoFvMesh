/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright held by original author
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Application
    mapConservativeFields

Description
    Maps volume fields conservatively from one mesh to another, reading and
    interpolating all fields present in the time directory of both cases.

Author
    Sandeep Menon
    University of Massachusetts Amherst
    All rights reserved

\*---------------------------------------------------------------------------*/

#include "fvCFD.H"
#include "IOobjectList.H"
#include "conservativeMeshToMesh.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

// Enumerants for testing
enum testType
{
    CONSTANT,
    LINEAR,
    SINUSOID_2D,
    SINUSOID_3D,
    COSINE_HILL_2D
};

// For constant-value tests, specify a value
const scalar constScalVal = 2.0;

int getTimeIndex
(
    const instantList& times,
    const scalar t
)
{
    int nearestIndex = -1;
    scalar nearestDiff = Foam::GREAT;

    forAll(times, timeIndex)
    {
        if (times[timeIndex].name() == "constant") continue;

        scalar diff = fabs(times[timeIndex].value() - t);
        if (diff < nearestDiff)
        {
            nearestDiff = diff;
            nearestIndex = timeIndex;
        }
    }

    return nearestIndex;
}


template<class Type>
void MapConservativeVolFields
(
    const IOobjectList& objects,
    const conservativeMeshToMesh& meshToMeshInterp,
    const label method
)
{
    const fvMesh& meshSource = meshToMeshInterp.srcMesh();
    const fvMesh& meshTarget = meshToMeshInterp.tgtMesh();

    word fieldClassName
    (
        GeometricField<Type, fvPatchField, volMesh>::typeName
    );

    IOobjectList fields = objects.lookupClass(fieldClassName);

    for
    (
        IOobjectList::iterator fieldIter = fields.begin();
        fieldIter != fields.end();
        ++fieldIter
    )
    {
        Info<< "    Interpolating " << fieldIter()->name() << endl;

        // Read field
        GeometricField<Type, fvPatchField, volMesh> fieldSource
        (
            *fieldIter(),
            meshSource
        );

        // Compute integral of source field
        Type intSource = gSum(meshSource.V() * fieldSource.internalField());

        Info<< "Integral source: " << intSource << endl;

        IOobject fieldTargetIOobject
        (
            fieldIter()->name(),
            meshTarget.time().timeName(),
            meshTarget,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        );

        Type intTarget = pTraits<Type>::zero;

        if (fieldTargetIOobject.headerOk())
        {
            // Read fieldTarget
            GeometricField<Type, fvPatchField, volMesh> fieldTarget
            (
                fieldTargetIOobject,
                meshTarget
            );

            // Interpolate field
            meshToMeshInterp.interpolate
            (
                fieldTarget,
                fieldSource,
                method
            );

            intTarget =
            (
                gSum(meshTarget.V() * fieldTarget.internalField())
            );

            // Write field
            fieldTarget.write();
        }
        else
        {
            fieldTargetIOobject.readOpt() = IOobject::NO_READ;

            // Interpolate field
            GeometricField<Type, fvPatchField, volMesh> fieldTarget
            (
                fieldTargetIOobject,
                meshToMeshInterp.interpolate(fieldSource, method)
            );

            intTarget =
            (
                gSum(meshTarget.V() * fieldTarget.internalField())
            );

            // Write field
            fieldTarget.write();
        }

        Info<< "Integral target: " << intTarget << endl;
        Info<< "mag(intError): " << mag(intSource - intTarget) << endl;
    }
}


// Initialise boundary fields for testing
void initBoundaryFields
(
    const fvMesh& mesh,
    const testType type,
    autoPtr<volScalarField>& tField
)
{
    // Fetch cell-centres
    const volVectorField& xC = mesh.C();

    // Fetch references
    volScalarField& field = tField();

    vector hC(0,0,0);
    scalar L = 0.5 * Foam::sqrt(2.0);
    scalar pi = mathematicalConstant::pi;

    switch (type)
    {
        case CONSTANT:
        {
            forAll(field.boundaryField(), patchI)
            {
                forAll(field.boundaryField()[patchI], faceI)
                {
                    field.boundaryField()[patchI][faceI] = constScalVal;
                }
            }

            break;
        }

        case LINEAR:
        {
            forAll(field.boundaryField(), patchI)
            {
                forAll(field.boundaryField()[patchI], faceI)
                {
                    const vector x = xC.boundaryField()[patchI][faceI];

                    field.boundaryField()[patchI][faceI] =
                    (
                        2.0*x.x() + 3.0*x.y() + x.z()
                    );
                }
            }

            break;
        }

        case SINUSOID_2D:
        {
            forAll(field.boundaryField(), patchI)
            {
                forAll(field.boundaryField()[patchI], faceI)
                {
                    const vector x = xC.boundaryField()[patchI][faceI];

                    field.boundaryField()[patchI][faceI] =
                    (
                        1.0
                      + Foam::sin(2.0*pi*x.x())
                      * Foam::sin(2.0*pi*x.y())
                    );
                }
            }

            break;
        }

        case SINUSOID_3D:
        {
            forAll(field.boundaryField(), patchI)
            {
                forAll(field.boundaryField()[patchI], faceI)
                {
                    const vector x = xC.boundaryField()[patchI][faceI];

                    field.boundaryField()[patchI][faceI] =
                    (
                        1.0
                      + Foam::sin(2.0*pi*x.x())
                      * Foam::sin(2.0*pi*x.y())
                      * Foam::sin(2.0*pi*x.z())
                    );
                }
            }

            break;
        }

        case COSINE_HILL_2D:
        {
            forAll(field.boundaryField(), patchI)
            {
                forAll(field.boundaryField()[patchI], faceI)
                {
                    scalar r = mag(xC.boundaryField()[patchI][faceI] - hC);

                    field.boundaryField()[patchI][faceI] =
                    (
                        2.0 + Foam::cos(pi*r/L)
                    );
                }
            }

            break;
        }

        default:
        {
            FatalErrorIn
            (
                "\n\n"
                "tmp<volScalarField> initBoundaryFields\n"
                "(\n"
                "    const fvMesh& mesh,\n"
                "    const testType type,\n"
                "    autoPtr<volScalarField>& tField\n"
                ")\n"
            )
                << "Unknown enumerant for initialisation."
                << abort(FatalError);
        }
    }
}


// Initialise a scalar field for testing
void initTestField
(
    const fvMesh& mesh,
    const testType type,
    autoPtr<volScalarField>& tField,
    autoPtr<volVectorField>& tgField,
    bool populate
)
{
    // Clear existing pointers
    tField.clear();
    tgField.clear();

    // Fetch cell-centres
    const volVectorField& xC = mesh.C();

    tField.set
    (
        new volScalarField
        (
            IOobject
            (
                "alpha",
                mesh.time().timeName(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            dimensioned<scalar>("alpha", dimless, 0.0),
            "fixedValue"
        )
    );

    tgField.set
    (
        new volVectorField
        (
            IOobject
            (
                "grad(alpha)",
                mesh.time().timeName(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            dimensioned<vector>("grad(alpha)", dimless, vector::zero),
            "zeroGradient"
        )
    );

    if (populate)
    {
        // Fetch references
        volScalarField& field = tField();
        volVectorField& gfield = tgField();

        vector hC(0,0,0);
        scalar L = 0.5 * Foam::sqrt(2.0);
        scalar pi = mathematicalConstant::pi;

        switch (type)
        {
            case CONSTANT:
            {
                // Test constant field
                forAll(field, cellI)
                {
                    field[cellI] = constScalVal;
                    gfield[cellI] = vector(0.0, 0.0, 0.0);
                }

                break;
            }

            case LINEAR:
            {
                // Test linear field
                forAll(field, cellI)
                {
                    const vector x = xC[cellI];

                    field[cellI] = 2.0*x.x() + 3.0*x.y() + x.z();
                    gfield[cellI] = vector(2.0, 3.0, 1.0);
                }

                break;
            }

            case SINUSOID_2D:
            {
                // Test 2D sinusoidal field
                forAll(field, cellI)
                {
                    const vector x = xC[cellI];

                    field[cellI] =
                    (
                        1.0
                      + Foam::sin(2.0*pi*x.x())
                      * Foam::sin(2.0*pi*x.y())
                    );

                    gfield[cellI] = 2.0 * pi *
                    (
                        vector
                        (
                            Foam::cos(2.0*pi*x.x()) * Foam::sin(2.0*pi*x.y()),
                            Foam::sin(2.0*pi*x.x()) * Foam::cos(2.0*pi*x.y()),
                            0.0
                        )
                    );
                }

                break;
            }

            case SINUSOID_3D:
            {
                // Test 3D sinusoidal field
                forAll(field, cellI)
                {
                    const vector x = xC[cellI];

                    field[cellI] =
                    (
                        1.0
                      + Foam::sin(2.0*pi*x.x())
                      * Foam::sin(2.0*pi*x.y())
                      * Foam::sin(2.0*pi*x.z())
                    );

                    gfield[cellI] = 2.0 * pi *
                    (
                        vector
                        (
                            Foam::cos(2.0*pi*x.x())
                          * Foam::sin(2.0*pi*x.y())
                          * Foam::sin(2.0*pi*x.z()),

                            Foam::sin(2.0*pi*x.x())
                          * Foam::cos(2.0*pi*x.y())
                          * Foam::sin(2.0*pi*x.z()),

                            Foam::sin(2.0*pi*x.x())
                          * Foam::sin(2.0*pi*x.y())
                          * Foam::cos(2.0*pi*x.z())
                        )
                    );
                }

                break;
            }

            case COSINE_HILL_2D:
            {
                forAll(field, cellI)
                {
                    const vector x = xC[cellI];

                    scalar r = mag(x - hC);

                    field[cellI] = 2.0 + Foam::cos(pi*r/L);

                    gfield[cellI] = (pi/(r*L)) *
                    (
                        vector
                        (
                            - Foam::sin(pi*r/L) * x.x(),
                            - Foam::sin(pi*r/L) * x.y(),
                            - Foam::sin(pi*r/L) * x.z()
                        )
                    );
                }

                break;
            }

            default:
            {
                FatalErrorIn
                (
                    "\n\n"
                    "tmp<volScalarField> initTestField\n"
                    "(\n"
                    "    const fvMesh& mesh,\n"
                    "    const testType type,\n"
                    "    bool populate\n"
                    ")\n"
                )
                    << "Unknown enumerant for initialisation."
                    << abort(FatalError);
            }
        }

        // Populate the boundary-fields
        initBoundaryFields(mesh, type, tField);
    }
}


// Compute interpolation error
void computeError
(
    const volScalarField& fieldSource,
    const volScalarField& fieldTarget,
    const testType type
)
{
    volScalarField iError
    (
        IOobject
        (
            "iError",
            fieldTarget.mesh().time().timeName(),
            fieldTarget.mesh(),
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        fieldTarget.mesh(),
        fieldTarget.dimensions()
    );

    const vectorField& sCentres = fieldSource.mesh().cellCentres();
    const vectorField& tCentres = fieldTarget.mesh().cellCentres();

    scalar sError = 0.0, tError = 0.0;
    scalar smError = 0.0, tmError = 0.0;

    const scalarField& isF = fieldSource.internalField();
    const scalarField& itF = fieldTarget.internalField();

    vector hC(0,0,0);
    scalar L = 0.5 * Foam::sqrt(2.0);
    scalar pi = mathematicalConstant::pi, sExact = 0.0, tExact = 0.0;

    forAll(isF, cellI)
    {
        const vector xC = sCentres[cellI];

        switch (type)
        {
            case CONSTANT:
            {
                sExact = constScalVal;

                break;
            }

            case LINEAR:
            {
                sExact = 2.0*xC.x() + 3.0*xC.y() + xC.z();

                break;
            }

            case SINUSOID_2D:
            {
                sExact =
                (
                    1.0
                  + Foam::sin(2.0*pi*xC.x())
                  * Foam::sin(2.0*pi*xC.y())
                );

                break;
            }

            case SINUSOID_3D:
            {
                sExact =
                (
                    1.0
                  + Foam::sin(2.0*pi*xC.x())
                  * Foam::sin(2.0*pi*xC.y())
                  * Foam::sin(2.0*pi*xC.z())
                );

                break;
            }

            case COSINE_HILL_2D:
            {
                scalar r = mag(xC - hC);

                sExact = 2.0 + Foam::cos(pi*r/L);

                break;
            }
        }

        sError += magSqr(isF[cellI] - sExact);
        smError = Foam::max(smError, mag(isF[cellI] - sExact));
    }

    forAll(itF, cellI)
    {
        const vector xC = tCentres[cellI];

        switch (type)
        {
            case CONSTANT:
            {
                tExact = constScalVal;

                break;
            }

            case LINEAR:
            {
                tExact = 2.0*xC.x() + 3.0*xC.y() + xC.z();

                break;
            }

            case SINUSOID_2D:
            {
                tExact =
                (
                    1.0
                  + Foam::sin(2.0*pi*xC.x())
                  * Foam::sin(2.0*pi*xC.y())
                );

                break;
            }

            case SINUSOID_3D:
            {
                tExact =
                (
                    1.0
                  + Foam::sin(2.0*pi*xC.x())
                  * Foam::sin(2.0*pi*xC.y())
                  * Foam::sin(2.0*pi*xC.z())
                );

                break;
            }

            case COSINE_HILL_2D:
            {
                scalar r = mag(xC - hC);

                tExact = 2.0 + Foam::cos(pi*r/L);

                break;
            }
        }

        tError += magSqr(itF[cellI] - tExact);
        tmError = Foam::max(tmError, mag(itF[cellI] - tExact));

        iError.internalField()[cellI] = mag(itF[cellI] - tExact);
    }

    // Compute mesh dx
    scalar sdx = Foam::cbrt(1.0 / isF.size());
    scalar tdx = Foam::cbrt(1.0 / itF.size());

    Info<< " ~~~~~~~~~~~~~~~~~ " << nl
        << "      Source       " << nl
        << " ~~~~~~~~~~~~~~~~~ " << nl
        << " L2 error: " << Foam::sqrt(sError/isF.size()) << nl
        << " Linf error: " << smError << nl
        << " dx: " << sdx << nl
        << " dx2: " << sqr(sdx) << nl
        << " nCells: " << isF.size() << nl
        << endl;

    Info<< " ~~~~~~~~~~~~~~~~~ " << nl
        << "      Target       " << nl
        << " ~~~~~~~~~~~~~~~~~ " << nl
        << " L2 error: " << Foam::sqrt(tError/itF.size()) << nl
        << " Linf error: " << tmError << nl
        << " dx: " << tdx << nl
        << " dx2: " << sqr(tdx) << nl
        << " nCells: " << itF.size() << nl
        << endl;
}


// Test routine to determine interpolation error
void testCyclicRemap
(
    const label nCycles,
    const testType type,
    const fvMesh& meshSource,
    const fvMesh& meshTarget,
    const label method,
    const label nThreads,
    const bool forceRecalc,
    const bool writeAddr
)
{
    // Initialize and populate fields
    autoPtr<volScalarField> fieldSource, fieldTarget;
    autoPtr<volVectorField> gfieldSource, gfieldTarget;

    initTestField
    (
        meshSource,
        type,
        fieldSource,
        gfieldSource,
        true
    );

    // Compute integral of source field
    scalar intSource = gSum(meshSource.V() * fieldSource->internalField());

    // Write out source field
    fieldSource().write();
    gfieldSource().write();

    initTestField
    (
        meshTarget,
        type,
        fieldTarget,
        gfieldTarget,
        true
    );

    // Create the interpolation scheme
    conservativeMeshToMesh meshSourceToTarget
    (
        meshSource,
        meshTarget,
        nThreads,
        forceRecalc,
        writeAddr
    );

    // Create the interpolation scheme
    conservativeMeshToMesh meshTargetToSource
    (
        meshTarget,
        meshSource,
        nThreads,
        forceRecalc,
        writeAddr
    );

    Info<< " Remapping for " << nCycles << " cycles..." << endl;

    // Perform initial map
    meshSourceToTarget.interpolate
    (
        fieldTarget(),
        fieldSource(),
        gfieldSource(),
        method
    );

    // Populate the boundary-fields
    initBoundaryFields(meshTarget, type, fieldTarget);
    initBoundaryFields(meshSource, type, fieldSource);

    // Now perform cyclic map
    for (label i = 1; i < nCycles; i++)
    {
        // Map back to source
        meshTargetToSource.interpolate
        (
            fieldSource(),
            fieldTarget(),
            gfieldTarget(),
            method
        );

        // Populate the boundary-fields
        initBoundaryFields(meshTarget, type, fieldTarget);
        initBoundaryFields(meshSource, type, fieldSource);

        // Map to target
        meshSourceToTarget.interpolate
        (
            fieldTarget(),
            fieldSource(),
            gfieldSource(),
            method
        );

        // Populate the boundary-fields
        initBoundaryFields(meshTarget, type, fieldTarget);
        initBoundaryFields(meshSource, type, fieldSource);
    }

    // Write out final field
    fieldTarget().write();

    // Compute interpolation error
    computeError(fieldSource, fieldTarget, type);

    Info<< " Done." << endl;

    Info<< "Integral source (before): " << intSource << endl;

    scalar intSourceAfter = gSum(meshSource.V() * fieldSource->internalField());
    scalar intTarget = gSum(meshTarget.V() * fieldTarget->internalField());

    Info<< "Integral source (after): " << intSourceAfter << endl;
    Info<< "Integral target: " << intTarget << endl;
    Info<< "mag(intError): " << mag(intSource - intTarget) << endl;
}


// Test routine to determine interpolation error
void testMappingError
(
    const testType type,
    const fvMesh& meshSource,
    const fvMesh& meshTarget,
    const label method,
    const label nThreads,
    const bool forceRecalc,
    const bool writeAddr
)
{
    // Initialize and populate fields
    autoPtr<volScalarField> fieldSource, fieldTarget;
    autoPtr<volVectorField> gfieldSource, gfieldTarget;

    initTestField
    (
        meshSource,
        type,
        fieldSource,
        gfieldSource,
        true
    );

    initTestField
    (
        meshTarget,
        type,
        fieldTarget,
        gfieldTarget,
        false
    );

    // Create the interpolation scheme
    conservativeMeshToMesh meshToMeshInterp
    (
        meshSource,
        meshTarget,
        nThreads,
        forceRecalc,
        writeAddr
    );

    // Interpolate field
    meshToMeshInterp.interpolate
    (
        fieldTarget(),
        fieldSource(),
        gfieldSource(),
        method
    );

    // Compute interpolation error
    computeError(fieldSource, fieldTarget, type);
}


void mapConservativeMesh
(
    const fvMesh& meshSource,
    const fvMesh& meshTarget,
    const label method,
    const label nThreads,
    const bool forceRecalc,
    const bool writeAddr
)
{
    // Create the interpolation scheme
    conservativeMeshToMesh meshToMeshInterp
    (
        meshSource,
        meshTarget,
        nThreads,
        forceRecalc,
        writeAddr
    );

    Info<< nl
        << "Conservatively creating and mapping fields for time "
        << meshSource.time().timeName() << nl << endl;

    {
        // Search for list of objects for this time
        IOobjectList objects(meshSource, meshSource.time().timeName());

        // Map volFields
        // ~~~~~~~~~~~~~
        MapConservativeVolFields<scalar>
        (
            objects,
            meshToMeshInterp,
            method
        );

        MapConservativeVolFields<vector>
        (
            objects,
            meshToMeshInterp,
            method
        );

        /*
        MapConservativeVolFields<tensor>
        (
            objects,
            meshToMeshInterp,
            conservativeMeshToMesh::CONSERVATIVE_FIRST_ORDER
        );

        MapConservativeVolFields<symmTensor>
        (
            objects,
            meshToMeshInterp,
            conservativeMeshToMesh::CONSERVATIVE_FIRST_ORDER
        );

        MapConservativeVolFields<sphericalTensor>
        (
            objects,
            meshToMeshInterp,
            conservativeMeshToMesh::CONSERVATIVE_FIRST_ORDER
        );
        */
    }
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main(int argc, char *argv[])
{
#   include "setRoots.H"

#   include "createTimes.H"

#   include "setTimeIndex.H"

    runTimeSource.setTime(sourceTimes[sourceTimeIndex], sourceTimeIndex);
    runTimeTarget.setTime(sourceTimes[sourceTimeIndex], sourceTimeIndex);

    Info<< "\nSource time: " << runTimeSource.value()
        << "\nTarget time: " << runTimeTarget.value()
        << endl;

    Info<< "Create meshes\n" << endl;

    fvMesh meshSource
    (
        IOobject
        (
            fvMesh::defaultRegion,
            runTimeSource.timeName(),
            runTimeSource
        )
    );

    fvMesh meshTarget
    (
        IOobject
        (
            fvMesh::defaultRegion,
            runTimeTarget.timeName(),
            runTimeTarget
        )
    );

    Info<< "Source mesh size: " << meshSource.nCells() << tab
        << "Target mesh size: " << meshTarget.nCells() << nl << endl;

    switch (method)
    {
        case conservativeMeshToMesh::CONSERVATIVE:
        {
            Info<< "Using method: CONSERVATIVE" << endl;

            break;
        }

        case conservativeMeshToMesh::INVERSE_DISTANCE:
        {
            Info<< "Using method: INVERSE_DISTANCE" << endl;

            break;
        }

        case conservativeMeshToMesh::CONSERVATIVE_FIRST_ORDER:
        {
            Info<< "Using method: CONSERVATIVE_FIRST_ORDER" << endl;

            break;
        }

        default:
        {
            FatalErrorIn("mapConservativeFields")
                << "unknown interpolation scheme " << method
                << exit(FatalError);
        }
    }

    if (testOnly)
    {
        testMappingError
        (
            LINEAR,
            meshSource,
            meshTarget,
            method,
            nThreads,
            forceRecalc,
            writeAddr
        );

        if (meshSource.nGeometricD() == 2)
        {
            testCyclicRemap
            (
                250,
                COSINE_HILL_2D,
                meshSource,
                meshTarget,
                method,
                nThreads,
                forceRecalc,
                writeAddr
            );
        }
    }
    else
    {
        mapConservativeMesh
        (
            meshSource,
            meshTarget,
            method,
            nThreads,
            forceRecalc,
            writeAddr
        );
    }

    Info<< "\nEnd\n" << endl;

    return 0;
}


// ************************************************************************* //
