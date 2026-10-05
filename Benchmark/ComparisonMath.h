#pragma once

inline bool comparison_inverse(const MATH::MATRIX4X4& input, MATH::MATRIX4X4& output)
{
    double augmented[4][8]{};
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
        {
            augmented[row][column] = input.values[row][column];
            augmented[row][column + 4] = row == column ? 1 : 0;
        }
    for (int column = 0; column < 4; ++column)
    {
        int pivot = column;
        for (int row = column + 1; row < 4; ++row)
            if (std::abs(augmented[row][column]) > std::abs(augmented[pivot][column])) pivot = row;
        if (std::abs(augmented[pivot][column]) < 1e-12) return false;
        for (int index = 0; index < 8; ++index) std::swap(augmented[pivot][index], augmented[column][index]);
        const double divisor = augmented[column][column];
        for (double& value : augmented[column]) value /= divisor;
        for (int row = 0; row < 4; ++row) if (row != column)
        {
            const double factor = augmented[row][column];
            for (int index = 0; index < 8; ++index) augmented[row][index] -= factor * augmented[column][index];
        }
    }
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            output.values[row][column] = static_cast<float>(augmented[row][column + 4]);
    return true;
}
