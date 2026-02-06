/**
 * @file mat.hpp
 * @brief A simple column-major matrix template with basic arithmetic operations and utilities for constructing common transformation matrices.
 * @details The mat template is designed for small fixed-size matrices commonly used in graphics applications (e.g. 4x4 transformation matrices). It provides basic arithmetic operations (addition, subtraction, scalar multiplication/division) and utilities for constructing identity matrices and accessing rows/columns. The matrix is stored in column-major order, which is common in graphics APIs.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/math/euler.hpp>
#include <catalyst/math/vec.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

/**
 * @namespace catalyst::math
 * @brief The catalyst::math namespace contains all the mathematical types and functions provided by the Catalyst Math library. This includes vector and matrix types, quaternion support, angle types with user-defined literals, projection matrix utilities, transformation functions, and more. By organizing all math-related functionality within this namespace, we can avoid naming conflicts and provide a clear structure for users of the library to access the various mathematical tools they need for graphics programming and game development.
 * @details The Catalyst Math library is designed to be efficient, easy to use, and compatible with modern C++ standards. It provides a comprehensive set of mathematical utilities that are commonly used in graphics applications, making it easier for developers to perform complex mathematical operations without having to implement them from scratch. By including the appropriate headers from the Catalyst Math library, users can access all the functionality they need for their projects.
 */
namespace catalyst::math
{

    /**
     * @struct mat
     * @tparam T Scalar type for the matrix elements (e.g. float, double).
     * @tparam Rows Number of rows in the matrix.
     * @tparam Cols Number of columns in the matrix.
     * @requires is_vec_scalar_v<T>
     * @brief A simple column-major matrix template with basic arithmetic operations and utilities for constructing common transformation matrices.
     * @details The mat template is designed for small fixed-size matrices commonly used in graphics applications (e.g. 4x4 transformation matrices). It provides basic arithmetic operations (addition, subtraction, scalar multiplication/division) and utilities for constructing identity matrices and accessing rows/columns. The matrix is stored in column-major order, which is common in graphics APIs.
     * @note The matrix is stored in column-major order, meaning that the elements are stored column by column. For example, for a 4x4 matrix, the first four elements in the initializer list correspond to the first column, the next four to the second column, and so on. This convention is consistent with how matrices are typically represented in graphics APIs and allows for efficient access to columns.
     */
    template <typename T, std::size_t Rows, std::size_t Cols>
        requires is_vec_scalar_v<T>
    struct mat
    {
        static_assert(Rows > 0 && Cols > 0, "mat<T,R,C>: R and C must be > 0");

        /**
         * @brief The scalar type used for the matrix elements (e.g. float, double).
         */
        using value_type = T;
        /**
         * @brief The number of rows in the matrix.
         */
        static constexpr std::size_t rows = Rows;
        /**
         * @brief The number of columns in the matrix.
         */
        static constexpr std::size_t cols = Cols;

        /**
         * @brief Type alias for a column vector of the matrix (size equal to the number of rows).
         */
        using col_type = vec<T, Rows>;
        /**
         * @brief Type alias for a row vector of the matrix (size equal to the number of columns).
         */
        using row_type = vec<T, Cols>;

        /**
         * @brief The matrix data stored as an array of column vectors. The matrix is stored in column-major order, meaning that the elements are stored column by column. For example, for a 4x4 matrix, the first four elements in the initializer list correspond to the first column, the next four to the second column, and so on. This convention is consistent with how matrices are typically represented in graphics APIs and allows for efficient access to columns.
         */
        std::array<col_type, Cols> c{};

        /**
         * @fn mat
         * @brief Default constructor initializes the matrix to all zeros.
         */
        constexpr mat() noexcept = default;

        /**
         * @fn mat
         * @brief Constructs a matrix from an initializer list of scalar values. The values are filled in column-major order, meaning that the first set of values corresponds to the first column, the next set to the second column, and so on. If the initializer list has fewer elements than the total number of matrix elements (Rows * Cols), the remaining elements are initialized to zero. If the initializer list has more elements than needed, the extra values are ignored.
         * @param init An initializer list of scalar values to fill the matrix. The values should be provided in column-major order.
         */
        constexpr mat(std::initializer_list<T> init) noexcept
        {
            // Count of elements to copy from the initializer list, which is the minimum of the initializer list size and the total number of matrix elements (Rows * Cols).
            // This ensures we don't read out of bounds if the initializer list has fewer elements than needed.
            const std::size_t count = (std::min)(init.size(), Rows * Cols);
            auto it = init.begin();
            for (std::size_t i = 0; i < count; ++i, ++it) // Fill the matrix elements in column-major order. The index i is used to determine the row and column for each element. The row is calculated as i % Rows (the remainder when i is divided by the number of rows), and the column is calculated as i / Rows (the integer division of i by the number of rows). This way, the first set of values fills the first column, the next set fills the second column, and so on.
            {
                const std::size_t r = i % Rows;
                const std::size_t col = i / Rows;
                (*this)(r, col) = *it;
            }
            for (std::size_t i = count; i < Rows * Cols; ++i) // Initialize remaining elements to zero if the initializer list had fewer elements than needed. The loop continues from where the previous loop left off (i = count) and fills the rest of the matrix with zeros using the same column-major indexing.
            {
                const std::size_t r = i % Rows;
                const std::size_t col = i / Rows;
                (*this)(r, col) = T{};
            }
        }

        /**
         * @fn operator()
         * @brief Accesses the element at the specified row and column of the matrix.
         * @param r The row index (0-based) of the element to access. Must be less than Rows.
         * @param col The column index (0-based) of the element to access. Must be less than Cols.
         * @return A reference to the element at the specified row and column, allowing for both reading and writing. The element is accessed using column-major indexing, where the row is calculated as r and the column is calculated as col.
         */
        [[nodiscard]] constexpr T &operator()(std::size_t r, std::size_t col) noexcept
        {
            return c[col][r];
        }

        /**
         * @fn operator() const
         * @brief Accesses the element at the specified row and column of the matrix (const version).
         * @param r The row index (0-based) of the element to access. Must be less than Rows.
         * @param col The column index (0-based) of the element to access. Must be less than Cols.
         * @return A const reference to the element at the specified row and column, allowing for read-only access. The element is accessed using column-major indexing, where the row is calculated as r and the column is calculated as col.
         */
        [[nodiscard]] constexpr const T &operator()(std::size_t r, std::size_t col) const noexcept
        {
            return c[col][r];
        }

        /**
         * @fn column
         * @brief Accesses the specified column of the matrix.
         * @param col The column index (0-based) to access. Must be less than Cols.
         * @return A reference to the column vector at the specified index, allowing for both reading and writing. The column is accessed directly from the array of column vectors, where c[col] gives access to the entire column as a vec<T, Rows>.
         */
        [[nodiscard]] constexpr col_type &column(std::size_t col) noexcept { return c[col]; }
        /**
         * @fn column const
         * @brief Accesses the specified column of the matrix (const version).
         * @param col The column index (0-based) to access. Must be less than Cols.
         * @return A const reference to the column vector at the specified index, allowing for read-only access. The column is accessed directly from the array of column vectors, where c[col] gives access to the entire column as a vec<T, Rows>.
         */
        [[nodiscard]] constexpr const col_type &column(std::size_t col) const noexcept { return c[col]; }

        /**
         * @fn row
         * @brief Accesses the specified row of the matrix.
         * @param r The row index (0-based) to access. Must be less than Rows.
         * @return A row vector representing the specified row of the matrix, allowing for read-only access. The row is constructed by iterating over each column and extracting the element at the specified row index from each column vector. The resulting row vector contains the elements from the specified row across all columns.
         */
        [[nodiscard]] constexpr row_type row(std::size_t r) const noexcept
        {
            row_type out{};
            for (std::size_t j = 0; j < Cols; ++j)
                out[j] = (*this)(r, j);
            return out;
        }

        /**
         * @fn from_cols
         * @brief Constructs a matrix from an array of column vectors.
         * @param cols An array of column vectors, where each column vector is of type vec<T, Rows>. The number of column vectors must be equal to Cols.
         * @return A mat constructed from the given column vectors, where each column of the matrix is set to the corresponding column vector from the input array. The resulting matrix will have its columns filled according to the provided column vectors.
         */
        [[nodiscard]] static constexpr mat from_cols(const std::array<col_type, Cols> &cols) noexcept
        {
            mat out{};
            out.c = cols;
            return out;
        }

        /**
         * @fn from_rows
         * @brief Constructs a matrix from an array of row vectors.
         * @param rows_in An array of row vectors, where each row vector is of type vec<T, Cols>. The number of row vectors must be equal to Rows.
         * @return A mat constructed from the given row vectors, where each row of the matrix is set to the corresponding row vector from the input array. The resulting matrix will have its rows filled according to the provided row vectors.
         */
        [[nodiscard]] static constexpr mat from_rows(const std::array<row_type, Rows> &rows_in) noexcept
        {
            mat out{};
            for (std::size_t r = 0; r < Rows; ++r)
            {
                for (std::size_t j = 0; j < Cols; ++j)
                    out(r, j) = rows_in[r][j];
            }
            return out;
        }

        /**
         * @fn identity
         * @brief Returns an identity matrix of the specified size. The identity matrix is a square matrix with ones on the main diagonal and zeros elsewhere. This function can only be called for square matrices (where Rows == Cols).
         * @return An identity matrix of size Rows x Cols, where the diagonal elements are set to 1 and all other elements are set to 0. The resulting matrix will have the property that when it is multiplied by any compatible matrix, it will return that same matrix (i.e. it acts as a multiplicative identity).
         * @throws static_assert If the function is called for a non-square matrix (where Rows != Cols), a compile-time error will occur with the message "mat::identity() requires a square matrix".
         */
        [[nodiscard]] static constexpr mat identity() noexcept
        {
            static_assert(Rows == Cols, "mat::identity() requires a square matrix");
            mat out{};
            for (std::size_t i = 0; i < Rows; ++i)
                out(i, i) = T{1};
            return out;
        }

        /**
         * @fn from_xywh
         * @brief Constructs a rectangle matrix from the given x, y, width, and height values. This function is a utility for creating a 2D transformation matrix that represents a rectangle defined by its top-left corner (x, y) and its size (width w and height h). The resulting matrix can be used for operations such as scaling and translation in 2D space.
         * @param x The x-coordinate of the top-left corner of the rectangle.
         * @param y The y-coordinate of the top-left corner of the rectangle.
         * @param w The width of the rectangle.
         * @param h The height of the rectangle.
         * @return A mat representing the rectangle defined by the given parameters. The matrix is constructed using the from_pos_size function, where the position is given by (x, y) and the size is given by (w, h). The resulting matrix will have its columns set according to these parameters, allowing for transformations that correspond to the specified rectangle.
         */
        [[nodiscard]] friend constexpr mat operator+(mat a, const mat &b) noexcept
        {
            for (std::size_t j = 0; j < Cols; ++j)
                a.c[j] += b.c[j];
            return a;
        }

        /**
         * @fn operator-
         * @brief Subtracts two matrices element-wise and returns the result.
         * @param a The first matrix operand.
         * @param b The second matrix operand to be subtracted from the first.
         * @return A new mat that is the result of element-wise subtraction of matrix b from matrix a. Each element of the resulting matrix is calculated as a(i, j) - b(i, j) for all valid row indices i and column indices j.
         */
        [[nodiscard]] friend constexpr mat operator-(mat a, const mat &b) noexcept
        {
            for (std::size_t j = 0; j < Cols; ++j)
                a.c[j] -= b.c[j];
            return a;
        }

        /**
         * @fn operator*
         * @brief Multiplies a matrix by a scalar value and returns the result.
         * @param a The matrix operand to be multiplied by the scalar.
         * @param s The scalar value to multiply the matrix by.
         * @return A new mat that is the result of multiplying each element of matrix a by the scalar value s. Each element of the resulting matrix is calculated as a(i, j) * s for all valid row indices i and column indices j.
         */
        [[nodiscard]] friend constexpr mat operator*(mat a, T s) noexcept
        {
            for (std::size_t j = 0; j < Cols; ++j)
                a.c[j] *= s;
            return a;
        }

        /**
         * @fn operator*
         * @brief Multiplies a scalar value by a matrix and returns the result.
         * @param s The scalar value to multiply the matrix by.
         * @param a The matrix operand to be multiplied by the scalar.
         * @return A new mat that is the result of multiplying each element of matrix a by the scalar value s. Each element of the resulting matrix is calculated as s * a(i, j) for all valid row indices i and column indices j. This operator allows for commutative scalar multiplication, meaning that the order of the operands does not affect the result (i.e. s * a is equivalent to a * s).
         */
        [[nodiscard]] friend constexpr mat operator*(T s, mat a) noexcept { return a * s; }

        /**
         * @fn operator/
         * @brief Divides a matrix by a scalar value and returns the result.
         * @param a The matrix operand to be divided by the scalar.
         * @param s The scalar value to divide the matrix by.
         * @return A new mat that is the result of dividing each element of matrix a by the scalar value s. Each element of the resulting matrix is calculated as a(i, j) / s for all valid row indices i and column indices j. This operation effectively scales down the matrix by the factor of s.
         */
        [[nodiscard]] friend constexpr mat operator/(mat a, T s) noexcept
        {
            for (std::size_t j = 0; j < Cols; ++j)
                a.c[j] /= s;
            return a;
        }

        /**
         * @fn operator+=
         * @brief Adds another matrix to this matrix in-place.
         * @param other The matrix to add to this matrix.
         * @return Reference to this mat after the addition, allowing for chaining of operations. The elements of this matrix are updated by adding the corresponding elements of the other matrix (i.e. (*this)(i, j) += other(i, j) for all valid row indices i and column indices j).
         */
        constexpr mat &operator+=(const mat &other) noexcept { return *this = (*this + other); }
        /**
         * @fn operator-=
         * @brief Subtracts another matrix from this matrix in-place.
         * @param other The matrix to subtract from this matrix.
         * @return Reference to this mat after the subtraction, allowing for chaining of operations. The elements of this matrix are updated by subtracting the corresponding elements of the other matrix (i.e. (*this)(i, j) -= other(i, j) for all valid row indices i and column indices j).
         */
        constexpr mat &operator-=(const mat &other) noexcept { return *this = (*this - other); }
        /**
         * @fn operator*=
         * @brief Multiplies this matrix by a scalar value in-place.
         * @param s The scalar value to multiply the matrix by.
         * @return Reference to this mat after the multiplication, allowing for chaining of operations. Each element of this matrix is updated by multiplying it by the scalar value s (i.e. (*this)(i, j) *= s for all valid row indices i and column indices j).
         */
        constexpr mat &operator*=(T s) noexcept { return *this = (*this * s); }
        /**
         * @fn operator/=
         * @brief Divides this matrix by a scalar value in-place.
         * @param s The scalar value to divide the matrix by.
         * @return Reference to this mat after the division, allowing for chaining of operations. Each element of this matrix is updated by dividing it by the scalar value s (i.e. (*this)(i, j) /= s for all valid row indices i and column indices j).
         */
        constexpr mat &operator/=(T s) noexcept { return *this = (*this / s); }
    };

    /**
     * @fn operator*
     * @brief Multiplies a matrix by a vector and returns the result. This operation is defined as the matrix-vector product, where the resulting vector is calculated by taking the dot product of each row of the matrix with the input vector. The number of columns in the matrix must match the size of the input vector for this operation to be valid.
     * @tparam T Scalar type for the matrix and vector elements (e.g. float, double).
     * @tparam R Number of rows in the matrix.
     * @tparam C Number of columns in the matrix (must match the size of the input vector).
     * @param m The matrix operand to be multiplied by the vector.
     * @param v The vector operand to be multiplied by the matrix.
     * @return The resulting vector from the matrix-vector multiplication. Each element of the resulting vector is calculated as the dot product of the corresponding row of the matrix with the input vector (i.e. out[i] = dot(m.row(i), v) for all valid row indices i). The size of the resulting vector will be equal to the number of rows in the matrix.
     */
    template <typename T, std::size_t R, std::size_t C>
    [[nodiscard]] constexpr vec<T, R> operator*(const mat<T, R, C> &m, const vec<T, C> &v) noexcept
    {
        vec<T, R> out{};
        for (std::size_t j = 0; j < C; ++j)
            out += m.c[j] * v[j];
        return out;
    }

    /**
     * @fn operator*
     * @brief Multiplies two matrices and returns the result. This operation is defined as the matrix product, where the resulting matrix is calculated by taking the dot product of the rows of the first matrix with the columns of the second matrix. The number of columns in the first matrix must match the number of rows in the second matrix for this operation to be valid.
     * @tparam T Scalar type for the matrix elements (e.g. float, double).
     * @tparam R Number of rows in the first matrix.
     * @tparam C Number of columns in the first matrix (must match the number of rows in the second matrix).
     * @tparam K Number of columns in the second matrix.
     * @param a The first matrix operand to be multiplied.
     * @param b The second matrix operand to be multiplied.
     * @return The resulting matrix from the multiplication of matrices a and b. Each element of the resulting matrix is calculated as the dot product of the corresponding row of matrix a with the corresponding column of matrix b (i.e. out(i, j) = dot(a.row(i), b.column(j)) for all valid row indices i and column indices j). The size of the resulting matrix will be R x K.
     */
    template <typename T, std::size_t R, std::size_t C, std::size_t K>
    [[nodiscard]] constexpr mat<T, R, K> operator*(const mat<T, R, C> &a, const mat<T, C, K> &b) noexcept
    {
        mat<T, R, K> out{};
        for (std::size_t j = 0; j < K; ++j)
            out.c[j] = a * b.c[j];
        return out;
    }

    /**
     * @typedef mat4f
     * @brief A 4x4 matrix with float elements, commonly used for transformations in graphics applications.
     */
    using mat4f = mat<float, 4, 4>;
    /**
     * @typedef mat3f
     * @brief A 3x3 matrix with float elements, commonly used for 2D transformations and normal matrices in graphics applications.
     */
    using mat3f = mat<float, 3, 3>;
    /**
     * @typedef mat2f
     * @brief A 2x2 matrix with float elements, commonly used for simple 2D transformations in graphics applications.
     */
    using mat2f = mat<float, 2, 2>;

    /**
     * @typedef mat4d
     * @brief A 4x4 matrix with double elements, commonly used for transformations in graphics applications that require higher precision.
     */
    using mat4d = mat<double, 4, 4>;
    /**
     * @typedef mat3d
     * @brief A 3x3 matrix with double elements, commonly used for 2D transformations and normal matrices in graphics applications that require higher precision.
     */
    using mat3d = mat<double, 3, 3>;
    /**
     * @typedef mat2d
     * @brief A 2x2 matrix with double elements, commonly used for simple 2D transformations in graphics applications that require higher precision.
     */
    using mat2d = mat<double, 2, 2>;

    /**
     * @fn rotation_yaw_pitch_roll
     * @brief Constructs a 3x3 rotation matrix from the given yaw, pitch, and roll angles. The rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis). This function is commonly used to create a rotation matrix that represents the orientation of an object in 3D space based on its Euler angles.
     * @param yaw The yaw angle (rotation around the Y-axis) in radians.
     * @param pitch The pitch angle (rotation around the X-axis) in radians.
     * @param roll The roll angle (rotation around the Z-axis) in radians.
     * @tparam T Scalar type for the matrix elements and angles (e.g. float, double).
     * @return A 3x3 rotation matrix that represents the combined rotation defined by the given yaw, pitch, and roll angles. The resulting matrix can be used to rotate vectors or other matrices in 3D space according to the specified Euler angles.
     */
    template <typename T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll(T yaw, T pitch, T roll) noexcept
    {
        const yaw_pitch_roll<T> a{yaw, pitch, roll};
        const auto t = detail::trig_full(a);

        mat<T, 3, 3> m{};

        // Row-major form, stored via m(row,col)
        m(0, 0) = t.cy * t.cp;
        m(0, 1) = (t.cy * t.sp * t.sr) - (t.sy * t.cr);
        m(0, 2) = (t.cy * t.sp * t.cr) + (t.sy * t.sr);

        m(1, 0) = t.sy * t.cp;
        m(1, 1) = (t.sy * t.sp * t.sr) + (t.cy * t.cr);
        m(1, 2) = (t.sy * t.sp * t.cr) - (t.cy * t.sr);

        m(2, 0) = -t.sp;
        m(2, 1) = t.cp * t.sr;
        m(2, 2) = t.cp * t.cr;

        return m;
    }

    /**
     * @fn rotation_yaw_pitch_roll
     * @brief Constructs a 3x3 rotation matrix from the given yaw, pitch, and roll angles specified as radians<T>. This is an overload of the rotation_yaw_pitch_roll function that accepts the angles wrapped in a radians<T> type, which provides type safety and clarity when working with angles. The rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis).
     * @param yaw The yaw angle (rotation around the Y-axis) wrapped in a radians<T> type.
     * @param pitch The pitch angle (rotation around the X-axis) wrapped in a radians<T> type.
     * @param roll The roll angle (rotation around the Z-axis) wrapped in a radians<T> type.
     * @tparam T Scalar type for the matrix elements and angles (e.g. float, double).
     * @return A 3x3 rotation matrix that represents the combined rotation defined by the given yaw, pitch, and roll angles. The resulting matrix can be used to rotate vectors or other matrices in 3D space according to the specified Euler angles. This overload allows for more explicit handling of angles by using the radians<T> type, which can help prevent confusion and errors when working with angle units.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll(radians<T> yaw, radians<T> pitch, radians<T> roll) noexcept
    {
        return rotation_yaw_pitch_roll(yaw.count(), pitch.count(), roll.count());
    }

    /**
     * @fn rotation_yaw_pitch_roll_degrees
     * @brief Constructs a 3x3 rotation matrix from the given yaw, pitch, and roll angles specified in degrees. This function converts the input angles from degrees to radians before calling the rotation_yaw_pitch_roll function to create the rotation matrix. The rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis).
     * @param yaw_deg The yaw angle (rotation around the Y-axis) in degrees.
     * @param pitch_deg The pitch angle (rotation around the X-axis) in degrees.
     * @param roll_deg The roll angle (rotation around the Z-axis) in degrees.
     * @tparam T Scalar type for the matrix elements and angles (e.g. float, double).
     * @return A 3x3 rotation matrix that represents the combined rotation defined by the given yaw, pitch, and roll angles. The resulting matrix can be used to rotate vectors or other matrices in 3D space according to the specified Euler angles. This function provides a convenient way to create a rotation matrix when working with angles in degrees, which are often more intuitive for human understanding and input.
     */
    template <typename T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll_degrees(T yaw_deg, T pitch_deg, T roll_deg) noexcept
    {
        const auto a = yaw_pitch_roll_from_degrees<T>(yaw_deg, pitch_deg, roll_deg);
        return rotation_yaw_pitch_roll(a);
    }

    /**
     * @fn rotation_yaw_pitch_roll_degrees
     * @brief Constructs a 3x3 rotation matrix from the given yaw, pitch, and roll angles specified as degrees<T>. This is an overload of the rotation_yaw_pitch_roll_degrees function that accepts the angles wrapped in a degrees<T> type, which provides type safety and clarity when working with angles. The function converts the input angles from degrees to radians before calling the rotation_yaw_pitch_roll function to create the rotation matrix. The rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis).
     * @param yaw_deg The yaw angle (rotation around the Y-axis) wrapped in a degrees<T> type.
     * @param pitch_deg The pitch angle (rotation around the X-axis) wrapped in a degrees<T> type.
     * @param roll_deg The roll angle (rotation around the Z-axis) wrapped in a degrees<T> type.
     * @tparam T Scalar type for the matrix elements and angles (e.g. float, double).
     * @return A 3x3 rotation matrix that represents the combined rotation defined by the given yaw, pitch, and roll angles. The resulting matrix can be used to rotate vectors or other matrices in 3D space according to the specified Euler angles. This overload allows for more explicit handling of angles by using the degrees<T> type, which can help prevent confusion and errors when working with angle units.
     */
    template <std::floating_point T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll_degrees(degrees<T> yaw_deg, degrees<T> pitch_deg, degrees<T> roll_deg) noexcept
    {
        const auto a = yaw_pitch_roll_from_degrees(yaw_deg, pitch_deg, roll_deg);
        return rotation_yaw_pitch_roll(a);
    }

    /**
     * @fn rotation_yaw_pitch_roll
     * @brief Constructs a 3x3 rotation matrix from the given yaw, pitch, and roll angles specified as a yaw_pitch_roll<T> struct. This is an overload of the rotation_yaw_pitch_roll function that accepts the angles wrapped in a yaw_pitch_roll<T> type, which provides a convenient way to pass all three angles together as a single argument. The rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis).
     * @param a A yaw_pitch_roll<T> struct containing the yaw, pitch, and roll angles in radians.
     * @tparam T Scalar type for the matrix elements and angles (e.g. float, double).
     * @return A 3x3 rotation matrix that represents the combined rotation defined by the given yaw, pitch, and roll angles. The resulting matrix can be used to rotate vectors or other matrices in 3D space according to the specified Euler angles. This overload allows for more convenient handling of Euler angles by using a single struct to encapsulate all three angles, which can improve code readability and organization when working with rotations.
     */
    template <typename T>
    [[nodiscard]] inline mat<T, 3, 3> rotation_yaw_pitch_roll(const yaw_pitch_roll<T> &a) noexcept
    {
        return rotation_yaw_pitch_roll(a.yaw, a.pitch, a.roll);
    }

    /**
     * @fn to_yaw_pitch_roll
     * @brief Extracts the yaw, pitch, and roll angles from a given 3x3 rotation matrix. This function assumes that the input matrix represents a rotation in 3D space and attempts to decompose it into its corresponding Euler angles (yaw, pitch, roll). The extraction is based on the standard convention for Euler angles, where the rotation is applied in the order of yaw (rotation around the Y-axis), then pitch (rotation around the X-axis), and finally roll (rotation around the Z-axis). The function also handles potential gimbal lock situations when the pitch angle is near ±90 degrees.
     * @param m The 3x3 rotation matrix from which to extract the yaw, pitch, and roll angles.
     * @tparam T Scalar type for the matrix elements and angles (e.g. float, double).
     * @return A yaw_pitch_roll<T> struct containing the extracted yaw, pitch, and roll angles in radians. The resulting angles represent the orientation of the rotation defined by the input matrix. If the input matrix is not a valid rotation matrix or if it is near gimbal lock, the extracted angles may not be unique or may have certain limitations in their representation.
     */
    template <typename T>
    [[nodiscard]] inline yaw_pitch_roll<T> to_yaw_pitch_roll(const mat<T, 3, 3> &m) noexcept
    {
        // For R = Rz(yaw) * Ry(pitch) * Rx(roll):
        // m(2,0) = -sin(pitch)
        // m(0,0) = cos(yaw)*cos(pitch)
        // m(1,0) = sin(yaw)*cos(pitch)
        // m(2,1) = cos(pitch)*sin(roll)
        // m(2,2) = cos(pitch)*cos(roll)

        const T sp = -m(2, 0);
        const T pitch = static_cast<T>(std::asin(static_cast<long double>(detail::clamp(sp, T{-1}, T{1}))));

        // If cos(pitch) is close to 0, we're near gimbal lock.
        const T cp = static_cast<T>(std::cos(static_cast<long double>(pitch)));

        T yaw{};
        T roll{};
        if (std::fabs(cp) > static_cast<T>(1e-6))
        {
            yaw = static_cast<T>(
                std::atan2(
                    static_cast<long double>(m(1, 0)),
                    static_cast<long double>(m(0, 0))));

            roll = static_cast<T>(
                std::atan2(
                    static_cast<long double>(m(2, 1)),
                    static_cast<long double>(m(2, 2))));
        }
        else
        {
            // Gimbal lock: roll is set to 0 and yaw is inferred from the remaining terms.
            yaw = static_cast<T>(
                std::atan2(
                    static_cast<long double>(-m(0, 1)),
                    static_cast<long double>(m(1, 1))));
            roll = T{};
        }

        return yaw_pitch_roll<T>{yaw, pitch, roll};
    }

} // namespace catalyst::math